#include "OnlineRoom/CC_RoomFramework.h"
#include "OnlineRoom/CC_OnlineRoomSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Misc/PackageName.h"
#include "UI/Framework/CC_ActivatableWidget.h"

void ACC_RoomPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACC_RoomPlayerState, bReady);
	DOREPLIFETIME(ACC_RoomPlayerState, bHost);
}

void ACC_RoomPlayerState::CopyProperties(APlayerState* NewState)
{
	Super::CopyProperties(NewState);
	if (ACC_RoomPlayerState* Target = Cast<ACC_RoomPlayerState>(NewState))
	{
		Target->bHost = bHost;
		Target->bReady = false;
		Target->CharacterConfig = CharacterConfig;
	}
}

void ACC_RoomPlayerState::SetReady(bool bValue)
{
	if (HasAuthority() && bReady != bValue) { bReady = bValue; ForceNetUpdate(); NotifyChanged(); }
}

void ACC_RoomPlayerState::SetHost(bool bValue)
{
	if (HasAuthority() && bHost != bValue) { bHost = bValue; ForceNetUpdate(); NotifyChanged(); }
}

void ACC_RoomPlayerState::NotifyChanged()
{
	OnMemberChanged.Broadcast();
	if (ACC_RoomLobbyState* Lobby = GetWorld()->GetGameState<ACC_RoomLobbyState>()) Lobby->NotifyChanged();
}

void ACC_RoomLobbyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACC_RoomLobbyState, Phase);
	DOREPLIFETIME(ACC_RoomLobbyState, Capacity);
}

void ACC_RoomLobbyState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyChanged();
}

void ACC_RoomLobbyState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyChanged();
}

TArray<ACC_RoomPlayerState*> ACC_RoomLobbyState::GetMembers() const
{
	TArray<ACC_RoomPlayerState*> Members;
	for (APlayerState* PS : PlayerArray)
		if (ACC_RoomPlayerState* Member = Cast<ACC_RoomPlayerState>(PS)) Members.Add(Member);
	return Members;
}

void ACC_RoomLobbyState::SetCapacity(int32 Value)
{
	if (HasAuthority()) { Capacity = FMath::Clamp(Value, 1, 64); ForceNetUpdate(); NotifyChanged(); }
}

void ACC_RoomLobbyState::SetPhase(ECC_RoomLobbyPhase Value)
{
	if (HasAuthority()) { Phase = Value; ForceNetUpdate(); NotifyChanged(); }
}

void ACC_RoomLobbyState::NotifyChanged() { OnLobbyChanged.Broadcast(); }

void ACC_RoomPlayerController::ServerSetReady_Implementation(bool bReady)
{
	ACC_RoomLobbyMode* Lobby = GetWorld()->GetAuthGameMode<ACC_RoomLobbyMode>();
	if (!Lobby || !Lobby->SetMemberReady(this, bReady))
		ClientRoomCommandFailed(NSLOCTEXT("CCRoom", "ReadyRejected", "Ready state cannot be changed now."));
}

void ACC_RoomPlayerController::ServerStartRoomGame_Implementation()
{
	FText Reason;
	ACC_RoomLobbyMode* Lobby = GetWorld()->GetAuthGameMode<ACC_RoomLobbyMode>();
	if (!Lobby) Reason = NSLOCTEXT("CCRoom", "NoLobby", "This is not a lobby.");
	if (!Lobby || !Lobby->StartRoomGame(this, Reason)) ClientRoomCommandFailed(Reason);
}

void ACC_RoomPlayerController::ClientRoomCommandFailed_Implementation(const FText& Reason)
{
	OnRoomCommandFailed(Reason);
}

void ACC_RoomPlayerController::ClientApplyRoomPhase_Implementation(bool bLobby)
{
	if (UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>())
		Rooms->NotifyRoomEntered();
	// 这里只同步房间业务；地图页面由独立的 UI 入口装配，不重建根布局。
}

ACC_RoomGameMode::ACC_RoomGameMode()
{
	bUseSeamlessTravel = true;
	PlayerControllerClass = ACC_RoomPlayerController::StaticClass();
	PlayerStateClass = ACC_RoomPlayerState::StaticClass();
}

bool ACC_RoomGameMode::CanAcceptPlayer() const
{
	const UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>();
	return IsLobbyMap() || (Rooms && Rooms->GetHostRequest().bAllowJoinInProgress);
}

void ACC_RoomGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	const UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>();
	const int32 Capacity = Rooms ? Rooms->GetHostRequest().MaxPlayers : 4;
	if (!CanAcceptPlayer()) ErrorMessage = TEXT("RoomNotJoinable");
	else if (GetNumPlayers() >= Capacity) ErrorMessage = TEXT("RoomFull");
}

void ACC_RoomGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// PreLogin 后到达的并发连接还要再次检查实际容量。
	const UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>();
	if ((IsLobbyMap() && !CanAcceptPlayer()) || GetNumPlayers() > (Rooms ? Rooms->GetHostRequest().MaxPlayers : 4))
	{
		NewPlayer->ClientReturnToMainMenuWithTextReason(NSLOCTEXT("CCRoom", "Full", "Room is full."));
		NewPlayer->Destroy();
		return;
	}
	if (!IsLobbyMap()) Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (ACC_RoomPlayerController* PC = Cast<ACC_RoomPlayerController>(NewPlayer)) PC->ClientApplyRoomPhase(IsLobbyMap());
}

bool ACC_RoomGameMode::ReturnToLobby()
{
	const UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>();
	if (!HasAuthority() || IsLobbyMap() || !Rooms) return false;
	const FString Map = Rooms->GetHostRequest().LobbyMap.ToSoftObjectPath().GetLongPackageName();
	return FPackageName::DoesPackageExist(Map) && GetWorld()->ServerTravel(Map);
}

ACC_RoomLobbyMode::ACC_RoomLobbyMode()
{
	GameStateClass = ACC_RoomLobbyState::StaticClass();
	DefaultPawnClass = nullptr;
}

void ACC_RoomLobbyMode::InitGameState()
{
	Super::InitGameState();
	const UCC_OnlineRoomSubsystem* Rooms = GetGameInstance()->GetSubsystem<UCC_OnlineRoomSubsystem>();
	if (ACC_RoomLobbyState* Lobby = GetGameState<ACC_RoomLobbyState>())
		Lobby->SetCapacity(Rooms ? Rooms->GetHostRequest().MaxPlayers : 4);
}

void ACC_RoomLobbyMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (ACC_RoomPlayerState* PS = NewPlayer->GetPlayerState<ACC_RoomPlayerState>())
	{
		// Listen Server 的本地控制器才是房主；不依赖容易发生竞争的登录顺序。
		PS->SetHost(NewPlayer->IsLocalController());
		PS->SetReady(false);
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

bool ACC_RoomLobbyMode::CanAcceptPlayer() const
{
	const ACC_RoomLobbyState* Lobby = GetGameState<ACC_RoomLobbyState>();
	return Lobby && Lobby->GetPhase() == ECC_RoomLobbyPhase::Waiting;
}

bool ACC_RoomLobbyMode::SetMemberReady(APlayerController* Requester, bool bReady)
{
	if (!Requester || Requester->GetWorld() != GetWorld() || !CanAcceptPlayer()) return false;
	ACC_RoomPlayerState* PS = Requester->GetPlayerState<ACC_RoomPlayerState>();
	if (!PS) return false;
	PS->SetReady(bReady);
	return true;
}

bool ACC_RoomLobbyMode::ValidateAdditionalStartRules_Implementation(FText& OutReason) const { return true; }

bool ACC_RoomLobbyMode::StartRoomGame(APlayerController* Requester, FText& OutReason)
{
	ACC_RoomLobbyState* Lobby = GetGameState<ACC_RoomLobbyState>();
	const ACC_RoomPlayerState* RequesterState = Requester ? Requester->GetPlayerState<ACC_RoomPlayerState>() : nullptr;
	if (!Lobby || !CanAcceptPlayer() || !RequesterState || !RequesterState->IsRoomHost()
		|| !Requester->IsLocalController())
	{
		OutReason = NSLOCTEXT("CCRoom", "HostOnly", "Only the host can start a waiting room.");
		return false;
	}
	const TArray<ACC_RoomPlayerState*> Members = Lobby->GetMembers();
	if (Members.Num() < FMath::Max(1, MinimumPlayers))
	{
		OutReason = NSLOCTEXT("CCRoom", "TooFew", "Not enough players.");
		return false;
	}
	for (const ACC_RoomPlayerState* Member : Members)
	{
		if (!Member->IsRoomHost() && !Member->IsReady())
		{
			OutReason = NSLOCTEXT("CCRoom", "NotReady", "All guests must be ready.");
			return false;
		}
	}
	const FString Map = GameMap.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::DoesPackageExist(Map))
	{
		OutReason = NSLOCTEXT("CCRoom", "NoGameMap", "The game map is not configured or missing.");
		return false;
	}
	if (!ValidateAdditionalStartRules(OutReason)) return false;
	Lobby->SetPhase(ECC_RoomLobbyPhase::Traveling);
	if (GetWorld()->ServerTravel(Map)) return true;
	Lobby->SetPhase(ECC_RoomLobbyPhase::Waiting);
	OutReason = NSLOCTEXT("CCRoom", "TravelRejected", "Server travel could not be started.");
	return false;
}
