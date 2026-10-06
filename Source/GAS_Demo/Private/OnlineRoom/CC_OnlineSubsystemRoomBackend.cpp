#include "OnlineRoom/CC_OnlineSubsystemRoomBackend.h"

#include "Engine/GameInstance.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

namespace CCRoomSessionKeys
{
	const FName Marker(TEXT("CC_ROOM"));
	const FName DisplayName(TEXT("CC_ROOM_NAME"));
	const FName GameMode(TEXT("CC_GAME_MODE"));
	const FName BuildId(TEXT("CC_BUILD_ID"));
}

FCC_OnlineSubsystemRoomBackend::~FCC_OnlineSubsystemRoomBackend()
{
	Shutdown();
}

void FCC_OnlineSubsystemRoomBackend::Initialize(UGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
	ResolveSessionInterface();
}

void FCC_OnlineSubsystemRoomBackend::Shutdown()
{
	ClearOnlineDelegates();
	ActiveSearch.Reset();
	SearchResultIndexByRoomId.Reset();
	SessionInterface.Reset();
	GameInstance.Reset();
	DestroyPurpose = EDestroyPurpose::None;
	OperationCompleteDelegate.Unbind();
	SearchCompleteDelegate.Unbind();
	JoinCompleteDelegate.Unbind();
}

IOnlineSessionPtr FCC_OnlineSubsystemRoomBackend::ResolveSessionInterface()
{
	if (!GameInstance.IsValid())
	{
		SessionInterface.Reset();
		return nullptr;
	}

	// 通过 World 查询，避免多 PIE 窗口共用错误的 OSS 实例。
	SessionInterface = Online::GetSessionInterface(GameInstance->GetWorld());
	return SessionInterface;
}

bool FCC_OnlineSubsystemRoomBackend::HostRoom(const FCC_RoomCreateRequest& Request)
{
	if (!ResolveSessionInterface().IsValid())
	{
		FailToStart(ECC_RoomOperation::Host, ECC_RoomResultCode::BackendUnavailable,
			TEXT("Online Session backend is unavailable."));
		return false;
	}

	// 重新建房前先清理同名 Session。这是一个小型 Command 链，可恢复 PIE 中的残留会话。
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		PendingCreateRequest = Request;
		return StartDestroySession(EDestroyPurpose::ReplaceBeforeCreate);
	}

	return StartCreateSession(Request);
}

bool FCC_OnlineSubsystemRoomBackend::StartCreateSession(const FCC_RoomCreateRequest& Request)
{
	if (!SessionInterface.IsValid())
	{
		FailToStart(ECC_RoomOperation::Host, ECC_RoomResultCode::BackendUnavailable,
			TEXT("Online Session backend is unavailable."));
		return false;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = Request.bIsLANMatch;
	Settings.NumPublicConnections = FMath::Max(1, Request.MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = Request.bShouldAdvertise;
	Settings.bAllowJoinInProgress = Request.bAllowJoinInProgress;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.BuildUniqueId = static_cast<int32>(GetTypeHash(Request.BuildId));

	const FString LobbyPackage = Request.LobbyMap.ToSoftObjectPath().GetLongPackageName();
	Settings.Set(CCRoomSessionKeys::Marker, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CCRoomSessionKeys::DisplayName, Request.DisplayName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CCRoomSessionKeys::GameMode, Request.GameModeId.ToString(),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CCRoomSessionKeys::BuildId, Request.BuildId,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, LobbyPackage, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateRaw(this, &ThisClass::HandleCreateSessionComplete));
	if (!SessionInterface->CreateSession(0, NAME_GameSession, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
		CreateDelegateHandle.Reset();
		FailToStart(ECC_RoomOperation::Host, ECC_RoomResultCode::UnknownFailure,
			TEXT("The backend rejected the create-session request."));
		return false;
	}
	return true;
}

bool FCC_OnlineSubsystemRoomBackend::FindRooms(const FCC_RoomSearchRequest& Request)
{
	if (!ResolveSessionInterface().IsValid())
	{
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::BackendUnavailable, FText::FromString(TEXT("Online Session backend is unavailable.")));
		SearchCompleteDelegate.ExecuteIfBound(Result, TArray<FCC_RoomSummary>());
		return false;
	}

	ActiveSearch = MakeShared<FOnlineSessionSearch>();
	SearchRequest = Request;
	SearchGeneration = FGuid::NewGuid().ToString();
	SearchResultIndexByRoomId.Reset();
	ActiveSearch->bIsLanQuery = Request.bIsLANQuery;
	ActiveSearch->MaxSearchResults = FMath::Clamp(Request.MaxResults, 1, 500);
	ActiveSearch->PingBucketSize = 50;
	ActiveSearch->QuerySettings.Set(CCRoomSessionKeys::Marker, true, EOnlineComparisonOp::Equals);
	if (!Request.bIsLANQuery)
	{
		ActiveSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	if (!Request.BuildId.IsEmpty())
	{
		ActiveSearch->QuerySettings.Set(CCRoomSessionKeys::BuildId, Request.BuildId, EOnlineComparisonOp::Equals);
	}

	FindDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateRaw(this, &ThisClass::HandleFindSessionsComplete));
	if (!SessionInterface->FindSessions(0, ActiveSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
		FindDelegateHandle.Reset();
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::UnknownFailure, FText::FromString(TEXT("The backend rejected the room search.")));
		SearchCompleteDelegate.ExecuteIfBound(Result, TArray<FCC_RoomSummary>());
		return false;
	}
	return true;
}

bool FCC_OnlineSubsystemRoomBackend::JoinRoom(const FString& RoomId)
{
	if (!ResolveSessionInterface().IsValid())
	{
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::BackendUnavailable, FText::FromString(TEXT("Online Session backend is unavailable.")));
		JoinCompleteDelegate.ExecuteIfBound(Result, FString());
		return false;
	}

	const int32* ResultIndex = SearchResultIndexByRoomId.Find(RoomId);
	if (!ResultIndex || !ActiveSearch.IsValid() || !ActiveSearch->SearchResults.IsValidIndex(*ResultIndex))
	{
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::RoomNotFound,
			FText::FromString(TEXT("The selected room is no longer present in the latest search results.")));
		JoinCompleteDelegate.ExecuteIfBound(Result, FString());
		return false;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::AlreadyInRoom,
			FText::FromString(TEXT("Leave the current room before joining another room.")));
		JoinCompleteDelegate.ExecuteIfBound(Result, FString());
		return false;
	}

	JoinDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateRaw(this, &ThisClass::HandleJoinSessionComplete));
	if (!SessionInterface->JoinSession(0, NAME_GameSession, ActiveSearch->SearchResults[*ResultIndex]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
		JoinDelegateHandle.Reset();
		const FCC_RoomOperationResult Result = FCC_RoomOperationResult::Failure(
			ECC_RoomResultCode::UnknownFailure, FText::FromString(TEXT("The backend rejected the join request.")));
		JoinCompleteDelegate.ExecuteIfBound(Result, FString());
		return false;
	}
	return true;
}

bool FCC_OnlineSubsystemRoomBackend::LeaveRoom()
{
	if (!ResolveSessionInterface().IsValid())
	{
		FailToStart(ECC_RoomOperation::Leave, ECC_RoomResultCode::BackendUnavailable,
			TEXT("Online Session backend is unavailable."));
		return false;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		OperationCompleteDelegate.ExecuteIfBound(ECC_RoomOperation::Leave,
			FCC_RoomOperationResult::Success(FText::FromString(TEXT("No active room needed cleanup."))));
		return true;
	}
	return StartDestroySession(EDestroyPurpose::Leave);
}

bool FCC_OnlineSubsystemRoomBackend::StartDestroySession(EDestroyPurpose Purpose)
{
	if (!SessionInterface.IsValid())
	{
		return false;
	}

	DestroyPurpose = Purpose;
	DestroyDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateRaw(this, &ThisClass::HandleDestroySessionComplete));
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
		DestroyDelegateHandle.Reset();
		DestroyPurpose = EDestroyPurpose::None;
		FailToStart(Purpose == EDestroyPurpose::Leave ? ECC_RoomOperation::Leave : ECC_RoomOperation::Host,
			ECC_RoomResultCode::UnknownFailure, TEXT("The backend rejected the destroy-session request."));
		return false;
	}
	return true;
}

bool FCC_OnlineSubsystemRoomBackend::HasSession() const
{
	return SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

void FCC_OnlineSubsystemRoomBackend::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession) return;
	if (SessionInterface.IsValid() && CreateDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
		CreateDelegateHandle.Reset();
	}

	const FCC_RoomOperationResult Result = bWasSuccessful
		? FCC_RoomOperationResult::Success(FText::FromString(TEXT("Room session created.")))
		: FCC_RoomOperationResult::Failure(ECC_RoomResultCode::UnknownFailure,
			FText::FromString(TEXT("Failed to create the room session.")));
	OperationCompleteDelegate.ExecuteIfBound(ECC_RoomOperation::Host, Result);
}

void FCC_OnlineSubsystemRoomBackend::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && FindDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
		FindDelegateHandle.Reset();
	}

	TArray<FCC_RoomSummary> Rooms;
	SearchResultIndexByRoomId.Reset();
	if (bWasSuccessful && ActiveSearch.IsValid())
	{
		for (int32 Index = 0; Index < ActiveSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& SearchResult = ActiveSearch->SearchResults[Index];
			if (!SearchResult.IsValid())
			{
				continue;
			}

			FCC_RoomSummary Summary;
			bool bOurRoom = false;
			SearchResult.Session.SessionSettings.Get(CCRoomSessionKeys::Marker, bOurRoom);
			FString FoundBuild;
			SearchResult.Session.SessionSettings.Get(CCRoomSessionKeys::BuildId, FoundBuild);
			// Null LAN 不保证执行所有 QuerySettings，客户端仍需验证命名空间和版本。
			if (!bOurRoom || (!SearchRequest.BuildId.IsEmpty() && FoundBuild != SearchRequest.BuildId)
				|| (SearchRequest.bHideFullRooms && SearchResult.Session.NumOpenPublicConnections <= 0)) continue;
			// 每轮搜索有独立令牌，旧 UI 行不能意外加入新一轮同索引的房间。
			Summary.RoomId = SearchGeneration + TEXT(":") + FString::FromInt(Index);
			if (Summary.RoomId.IsEmpty() || SearchResultIndexByRoomId.Contains(Summary.RoomId))
			{
				Summary.RoomId = FString::Printf(TEXT("SearchResult_%d"), Index);
			}
			SearchResult.Session.SessionSettings.Get(CCRoomSessionKeys::DisplayName, Summary.DisplayName);
			Summary.HostName = SearchResult.Session.OwningUserName;
			FString MapName;
			SearchResult.Session.SessionSettings.Get(SETTING_MAPNAME, MapName);
			Summary.MapId = FName(*MapName);
			FString GameMode;
			SearchResult.Session.SessionSettings.Get(CCRoomSessionKeys::GameMode, GameMode);
			Summary.GameModeId = FName(*GameMode);
			SearchResult.Session.SessionSettings.Get(CCRoomSessionKeys::BuildId, Summary.BuildId);
			Summary.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			Summary.CurrentPlayers = FMath::Max(0,
				Summary.MaxPlayers - SearchResult.Session.NumOpenPublicConnections);
			Summary.PingMs = SearchResult.PingInMs;
			Summary.bJoinable = SearchResult.Session.NumOpenPublicConnections > 0;

			SearchResultIndexByRoomId.Add(Summary.RoomId, Index);
			Rooms.Add(MoveTemp(Summary));
		}
	}

	const FCC_RoomOperationResult Result = bWasSuccessful
		? FCC_RoomOperationResult::Success(FText::FromString(TEXT("Room search completed.")))
		: FCC_RoomOperationResult::Failure(ECC_RoomResultCode::UnknownFailure,
			FText::FromString(TEXT("Failed to search for rooms.")));
	SearchCompleteDelegate.ExecuteIfBound(Result, Rooms);
}

void FCC_OnlineSubsystemRoomBackend::HandleJoinSessionComplete(
	FName SessionName, EOnJoinSessionCompleteResult::Type ResultCode)
{
	if (SessionName != NAME_GameSession) return;
	if (SessionInterface.IsValid() && JoinDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
		JoinDelegateHandle.Reset();
	}

	if (ResultCode == EOnJoinSessionCompleteResult::Success && SessionInterface.IsValid())
	{
		FString ConnectString;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectString) && !ConnectString.IsEmpty())
		{
			JoinCompleteDelegate.ExecuteIfBound(
				FCC_RoomOperationResult::Success(FText::FromString(TEXT("Joined room session."))), ConnectString);
			return;
		}
		JoinCompleteDelegate.ExecuteIfBound(
			FCC_RoomOperationResult::Failure(ECC_RoomResultCode::NetworkFailure,
				FText::FromString(TEXT("The room address could not be resolved."))), FString());
		return;
	}

	ECC_RoomResultCode StableCode = ECC_RoomResultCode::UnknownFailure;
	if (ResultCode == EOnJoinSessionCompleteResult::SessionIsFull)
	{
		StableCode = ECC_RoomResultCode::RoomFull;
	}
	else if (ResultCode == EOnJoinSessionCompleteResult::SessionDoesNotExist)
	{
		StableCode = ECC_RoomResultCode::RoomNotFound;
	}
	else if (ResultCode == EOnJoinSessionCompleteResult::AlreadyInSession)
	{
		StableCode = ECC_RoomResultCode::AlreadyInRoom;
	}
	else if (ResultCode == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress)
	{
		StableCode = ECC_RoomResultCode::NetworkFailure;
	}

	JoinCompleteDelegate.ExecuteIfBound(
		FCC_RoomOperationResult::Failure(StableCode,
			FText::FromString(FString::Printf(TEXT("Join room failed: %s"), LexToString(ResultCode)))), FString());
}

void FCC_OnlineSubsystemRoomBackend::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession) return;
	if (SessionInterface.IsValid() && DestroyDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
		DestroyDelegateHandle.Reset();
	}

	const EDestroyPurpose CompletedPurpose = DestroyPurpose;
	DestroyPurpose = EDestroyPurpose::None;
	if (CompletedPurpose == EDestroyPurpose::ReplaceBeforeCreate)
	{
		if (bWasSuccessful)
		{
			StartCreateSession(PendingCreateRequest);
		}
		else
		{
			FailToStart(ECC_RoomOperation::Host, ECC_RoomResultCode::UnknownFailure,
				TEXT("The previous room session could not be cleaned up."));
		}
		return;
	}

	const FCC_RoomOperationResult Result = bWasSuccessful
		? FCC_RoomOperationResult::Success(FText::FromString(TEXT("Left room session.")))
		: FCC_RoomOperationResult::Failure(ECC_RoomResultCode::UnknownFailure,
			FText::FromString(TEXT("Failed to leave the room session.")));
	OperationCompleteDelegate.ExecuteIfBound(ECC_RoomOperation::Leave, Result);
}

void FCC_OnlineSubsystemRoomBackend::ClearOnlineDelegates()
{
	if (!SessionInterface.IsValid())
	{
		return;
	}
	if (CreateDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
		CreateDelegateHandle.Reset();
	}
	if (FindDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
		FindDelegateHandle.Reset();
	}
	if (JoinDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
		JoinDelegateHandle.Reset();
	}
	if (DestroyDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
		DestroyDelegateHandle.Reset();
	}
}

void FCC_OnlineSubsystemRoomBackend::FailToStart(
	ECC_RoomOperation Operation, ECC_RoomResultCode Code, const TCHAR* Message)
{
	OperationCompleteDelegate.ExecuteIfBound(Operation,
		FCC_RoomOperationResult::Failure(Code, FText::FromString(Message)));
}
