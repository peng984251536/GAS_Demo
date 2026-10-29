#include "Game/Survival/CC_SurvivalPlayerState.h"

#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

void ACC_SurvivalPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACC_SurvivalPlayerState, Stats);
	DOREPLIFETIME(ACC_SurvivalPlayerState, LifeState);
	DOREPLIFETIME(ACC_SurvivalPlayerState, RespawnServerTime);
}

void ACC_SurvivalPlayerState::CopyProperties(APlayerState* NewState)
{
	// 父类复制房主标志与角色配置；统计、生命状态属于本局，不带入下一局。
	Super::CopyProperties(NewState);
}

float ACC_SurvivalPlayerState::GetRespawnRemainingSeconds() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (RespawnServerTime <= 0.0 || !GameState)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(RespawnServerTime - GameState->GetServerWorldTimeSeconds()));
}

void ACC_SurvivalPlayerState::AddStat(FGameplayTag Tag, int32 Count)
{
	if (!HasAuthority() || Count == 0)
	{
		return;
	}
	Stats.AddStack(Tag, Count);
	NotifySurvivalStateChanged();
}

void ACC_SurvivalPlayerState::SetLifeState(ECC_PlayerLifeState NewState)
{
	if (!HasAuthority() || LifeState == NewState)
	{
		return;
	}
	LifeState = NewState;
	NotifySurvivalStateChanged();
}

void ACC_SurvivalPlayerState::SetRespawnServerTime(double ServerTime)
{
	if (!HasAuthority() || RespawnServerTime == ServerTime)
	{
		return;
	}
	RespawnServerTime = ServerTime;
	NotifySurvivalStateChanged();
}

void ACC_SurvivalPlayerState::ResetForMatch()
{
	if (!HasAuthority())
	{
		return;
	}
	Stats.Reset();
	LifeState = ECC_PlayerLifeState::Alive;
	RespawnServerTime = 0.0;
	NotifySurvivalStateChanged();
}

void ACC_SurvivalPlayerState::NotifySurvivalStateChanged()
{
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
	OnSurvivalStateChanged.Broadcast();
}

void ACC_SurvivalPlayerState::OnRep_SurvivalState()
{
	OnSurvivalStateChanged.Broadcast();
}

bool ACC_SurvivalPlayerState::CanControlMatch() const
{
	if (IsRoomHost())
	{
		return true;
	}
	// 单机或监听服务器主机：本地控制器在服务器上。
	const APlayerController* Controller = GetPlayerController();
	return Controller && Controller->IsLocalController() && GetNetMode() != NM_Client;
}

void ACC_SurvivalPlayerState::ServerRequestPlayAgain_Implementation()
{
	if (!CanControlMatch())
	{
		return;
	}
	if (ACC_SurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ACC_SurvivalGameMode>())
	{
		Mode->RequestPlayAgain();
	}
}

void ACC_SurvivalPlayerState::ServerRequestReturnToLobby_Implementation()
{
	if (!CanControlMatch())
	{
		return;
	}
	if (ACC_SurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ACC_SurvivalGameMode>())
	{
		Mode->RequestReturnToLobby();
	}
}
