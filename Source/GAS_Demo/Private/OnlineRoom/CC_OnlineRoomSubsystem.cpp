#include "OnlineRoom/CC_OnlineRoomSubsystem.h"

#include "OnlineRoom/CC_OnlineSubsystemRoomBackend.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Misc/PackageName.h"

TUniquePtr<ICC_RoomSessionBackend> UCC_OnlineRoomSubsystem::CreateBackend()
{
	return MakeUnique<FCC_OnlineSubsystemRoomBackend>();
}

bool UCC_OnlineRoomSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> Children;
	GetDerivedClasses(GetClass(), Children, false);
	return Children.IsEmpty() && Super::ShouldCreateSubsystem(Outer);
}

bool UCC_OnlineRoomSubsystem::Reject(ECC_RoomOperation Operation, ECC_RoomResultCode Code, const TCHAR* Message)
{
	if (bDispatching) return false;
	TGuardValue<bool> Guard(bDispatching, true);
	LastResult = FCC_RoomOperationResult::Failure(Code, FText::FromString(Message));
	OnOperationCompleted.Broadcast(Operation, LastResult);
	return false;
}

void UCC_OnlineRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Backend = CreateBackend();
	if (!Backend) return;
	Backend->Initialize(GetGameInstance());
	Backend->OnOperationComplete().BindUObject(this, &ThisClass::HandleBackendOperation);
	Backend->OnSearchComplete().BindUObject(this, &ThisClass::HandleSearchComplete);
	Backend->OnJoinComplete().BindUObject(this, &ThisClass::HandleJoinComplete);
	TravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickTravelTimeout), 1.0f);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMap);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UCC_OnlineRoomSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TravelTickerHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (Backend)
	{
		Backend->Shutdown();
		Backend.Reset();
	}
	CachedRooms.Reset();
	Super::Deinitialize();
}

bool UCC_OnlineRoomSubsystem::HostRoom(const FCC_RoomCreateRequest& Request)
{
	if (Request.MaxPlayers < 1 || Request.MaxPlayers > 64 || Request.BuildId.IsEmpty()
		|| !FPackageName::DoesPackageExist(Request.LobbyMap.ToSoftObjectPath().GetLongPackageName()))
	{
		return Reject(ECC_RoomOperation::Host, ECC_RoomResultCode::InvalidRequest,
			TEXT("An existing lobby map, build id and 1-64 player slots are required."));
	}
	if (!BeginOperation(ECC_RoomOperation::Host, ECC_RoomAsyncState::Creating))
	{
		return false;
	}

	PendingHostRequest = Request;
	return Backend && Backend->HostRoom(Request);
}

bool UCC_OnlineRoomSubsystem::FindRooms(const FCC_RoomSearchRequest& Request)
{
	if (!BeginOperation(ECC_RoomOperation::Search, ECC_RoomAsyncState::Searching))
	{
		return false;
	}
	CachedRooms.Reset();
	return Backend && Backend->FindRooms(Request);
}

bool UCC_OnlineRoomSubsystem::JoinRoom(const FString& RoomId)
{
	if (RoomId.IsEmpty())
	{
		return Reject(ECC_RoomOperation::Join, ECC_RoomResultCode::InvalidRequest, TEXT("A room id is required."));
	}
	if (!BeginOperation(ECC_RoomOperation::Join, ECC_RoomAsyncState::Joining))
	{
		return false;
	}
	return Backend && Backend->JoinRoom(RoomId);
}

bool UCC_OnlineRoomSubsystem::LeaveRoom(TSoftObjectPtr<UWorld> FrontEndMap)
{
	if (!FPackageName::DoesPackageExist(FrontEndMap.ToSoftObjectPath().GetLongPackageName()))
		return Reject(ECC_RoomOperation::Leave, ECC_RoomResultCode::InvalidRequest, TEXT("An existing front-end map is required."));
	if (!BeginOperation(ECC_RoomOperation::Leave, ECC_RoomAsyncState::Destroying))
	{
		return false;
	}
	PendingFrontEndMap = FrontEndMap;
	return Backend && Backend->LeaveRoom();
}

bool UCC_OnlineRoomSubsystem::IsBusy() const
{
	return State != ECC_RoomAsyncState::Idle && State != ECC_RoomAsyncState::InRoom
		&& State != ECC_RoomAsyncState::RecoveryRequired;
}

bool UCC_OnlineRoomSubsystem::BeginOperation(ECC_RoomOperation Operation, ECC_RoomAsyncState NewState)
{
	if (bDispatching) return false;
	const bool bCanStart = State == ECC_RoomAsyncState::Idle
		|| (Operation == ECC_RoomOperation::Leave && (State == ECC_RoomAsyncState::InRoom
			|| State == ECC_RoomAsyncState::RecoveryRequired));
	if (!bCanStart || !Backend)
	{
		return Reject(Operation, !Backend ? ECC_RoomResultCode::BackendUnavailable : ECC_RoomResultCode::Busy,
			TEXT("The room operation is unavailable in the current state."));
	}

	ActiveOperation = Operation;
	SetState(NewState);
	return true;
}

void UCC_OnlineRoomSubsystem::SetState(ECC_RoomAsyncState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	TGuardValue<bool> Guard(bDispatching, true);
	OnStateChanged.Broadcast(State);
}

void UCC_OnlineRoomSubsystem::FinishOperation(
	ECC_RoomOperation Operation, const FCC_RoomOperationResult& Result)
{
	TGuardValue<bool> Guard(bDispatching, true);
	ActiveOperation = ECC_RoomOperation::None;
	PendingTravel = EPendingTravel::None;
	LastResult = Result;
	SetState(Result.bSuccess && (Operation == ECC_RoomOperation::Host || Operation == ECC_RoomOperation::Join)
		? ECC_RoomAsyncState::InRoom : (!Result.bSuccess && Backend && Backend->HasSession()
			? ECC_RoomAsyncState::RecoveryRequired : ECC_RoomAsyncState::Idle));
	OnOperationCompleted.Broadcast(Operation, Result);
}

void UCC_OnlineRoomSubsystem::HandleBackendOperation(
	ECC_RoomOperation Operation, const FCC_RoomOperationResult& Result)
{
	if (ActiveOperation != Operation) return;
	if (!Result.bSuccess)
	{
		FinishOperation(Operation, Result);
		return;
	}

	if (Operation == ECC_RoomOperation::Host)
	{
		SetState(ECC_RoomAsyncState::Traveling);
		if (!TravelToLocalMap(PendingHostRequest.LobbyMap, TEXT("listen"), EPendingTravel::EnterRoom))
		{
			FinishOperation(Operation, FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure,
				FText::FromString(TEXT("The lobby map could not be opened."))));
		}
		return;
	}

	if (Operation == ECC_RoomOperation::Leave)
	{
		if (PendingFrontEndMap.IsNull())
		{
			FinishOperation(Operation, Result);
			return;
		}
		SetState(ECC_RoomAsyncState::Traveling);
		if (!TravelToLocalMap(PendingFrontEndMap, FString(), EPendingTravel::ReturnToFrontEnd))
		{
			FinishOperation(Operation, FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure,
				FText::FromString(TEXT("The front-end map could not be opened."))));
		}
	}
}

void UCC_OnlineRoomSubsystem::HandleSearchComplete(
	const FCC_RoomOperationResult& Result, const TArray<FCC_RoomSummary>& Rooms)
{
	if (ActiveOperation != ECC_RoomOperation::Search) return;
	TGuardValue<bool> Guard(bDispatching, true);
	LastResult = Result;
	CachedRooms = Rooms;
	SetState(ECC_RoomAsyncState::Idle);
	ActiveOperation = ECC_RoomOperation::None;
	OnSearchCompleted.Broadcast(Result, CachedRooms);
	OnOperationCompleted.Broadcast(ECC_RoomOperation::Search, Result);
}

void UCC_OnlineRoomSubsystem::HandleJoinComplete(
	const FCC_RoomOperationResult& Result, const FString& ConnectString)
{
	if (ActiveOperation != ECC_RoomOperation::Join) return;
	if (!Result.bSuccess)
	{
		FinishOperation(ECC_RoomOperation::Join, Result);
		return;
	}

	APlayerController* LocalController = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!LocalController || ConnectString.IsEmpty())
	{
		FinishOperation(ECC_RoomOperation::Join,
			FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure,
				FText::FromString(TEXT("No local player controller or room address is available."))));
		return;
	}

	PendingTravel = EPendingTravel::EnterRoom;
	TravelDeadline = FPlatformTime::Seconds() + 60.0;
	SetState(ECC_RoomAsyncState::Traveling);
	LocalController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

bool UCC_OnlineRoomSubsystem::TravelToLocalMap(
	const TSoftObjectPtr<UWorld>& Map, const FString& Options, EPendingTravel TravelPurpose)
{
	const FString PackageName = Map.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::DoesPackageExist(PackageName) || !GetGameInstance())
	{
		return false;
	}
	PendingTravel = TravelPurpose;
	TravelDeadline = FPlatformTime::Seconds() + 60.0;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*PackageName), true, Options);
	return true;
}

void UCC_OnlineRoomSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()
		|| PendingTravel != EPendingTravel::ReturnToFrontEnd)
	{
		return;
	}
	TGuardValue<bool> Guard(bDispatching, true);

	const EPendingTravel CompletedTravel = PendingTravel;
	PendingTravel = EPendingTravel::None;
	const ECC_RoomOperation CompletedOperation = ActiveOperation;
	ActiveOperation = ECC_RoomOperation::None;
	SetState(CompletedTravel == EPendingTravel::EnterRoom
		? ECC_RoomAsyncState::InRoom : ECC_RoomAsyncState::Idle);
	LastResult = FCC_RoomOperationResult::Success(CompletedTravel == EPendingTravel::EnterRoom
			? FText::FromString(TEXT("Entered the room."))
			: FText::FromString(TEXT("Returned to the front end.")));
	OnOperationCompleted.Broadcast(CompletedOperation, LastResult);
}

void UCC_OnlineRoomSubsystem::NotifyRoomEntered()
{
	if (PendingTravel == EPendingTravel::EnterRoom
		&& (ActiveOperation == ECC_RoomOperation::Host || ActiveOperation == ECC_RoomOperation::Join))
		FinishOperation(ActiveOperation, FCC_RoomOperationResult::Success(FText::FromString(TEXT("Entered the room."))));
}

bool UCC_OnlineRoomSubsystem::TickTravelTimeout(float DeltaSeconds)
{
	if (State == ECC_RoomAsyncState::Traveling && FPlatformTime::Seconds() > TravelDeadline)
		FinishOperation(ActiveOperation, FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure,
			FText::FromString(TEXT("Travel timed out. Check the map's Room GameMode and clean up with LeaveRoom."))));
	return true;
}

void UCC_OnlineRoomSubsystem::HandleNetworkFailure(
	UWorld* FailedWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!FailedWorld || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// 监听服务器上单个客机断线只影响该成员，不应把房主的整个 Session 标为失败。
	if (FailedWorld->GetNetMode() == NM_ListenServer
		&& (FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout)) return;
	if (State == ECC_RoomAsyncState::Idle && (!Backend || !Backend->HasSession())) return;
	const ECC_RoomOperation FailedOperation = ActiveOperation;
	FinishOperation(FailedOperation,
		FCC_RoomOperationResult::Failure(ECC_RoomResultCode::NetworkFailure,
			FText::FromString(ErrorString.IsEmpty() ? TEXT("The network connection failed.") : ErrorString)));
}

void UCC_OnlineRoomSubsystem::HandleTravelFailure(
	UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!FailedWorld || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (State == ECC_RoomAsyncState::Idle && (!Backend || !Backend->HasSession())) return;
	const ECC_RoomOperation FailedOperation = ActiveOperation;
	FinishOperation(FailedOperation,
		FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure,
			FText::FromString(ErrorString.IsEmpty() ? TEXT("Map travel failed.") : ErrorString)));
}
