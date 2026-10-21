#include "Game/Survival/CC_SurvivalGameState.h"

#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_TeamProgressionComponent.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/CC_Tags.h"
#include "GAS_Demo.h"
#include "Net/UnrealNetwork.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"

ACC_SurvivalGameState::ACC_SurvivalGameState()
{
	WaveDirector = CreateDefaultSubobject<UCC_WaveDirectorComponent>(TEXT("WaveDirector"));
	Progression = CreateDefaultSubobject<UCC_TeamProgressionComponent>(TEXT("Progression"));
	ResultScreenLayer = CCTags::UILayer::Menu;
}

void ACC_SurvivalGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACC_SurvivalGameState, Phase);
	DOREPLIFETIME(ACC_SurvivalGameState, MatchStartServerTime);
	DOREPLIFETIME(ACC_SurvivalGameState, MatchEndServerTime);
	DOREPLIFETIME(ACC_SurvivalGameState, TeamStats);
}

float ACC_SurvivalGameState::GetMatchElapsedSeconds() const
{
	if (MatchStartServerTime <= 0.0)
	{
		return 0.f;
	}
	const double End = MatchEndServerTime > 0.0 ? MatchEndServerTime : GetServerWorldTimeSeconds();
	return FMath::Max(0.f, static_cast<float>(End - MatchStartServerTime));
}

float ACC_SurvivalGameState::GetSecondsUntilServerTime(double ServerTime) const
{
	return ServerTime <= 0.0 ? 0.f : FMath::Max(0.f, static_cast<float>(ServerTime - GetServerWorldTimeSeconds()));
}

TArray<ACC_SurvivalPlayerState*> ACC_SurvivalGameState::GetSurvivalPlayers() const
{
	TArray<ACC_SurvivalPlayerState*> Result;
	for (APlayerState* PS : PlayerArray)
	{
		ACC_SurvivalPlayerState* SurvivalPS = Cast<ACC_SurvivalPlayerState>(PS);
		if (SurvivalPS && !SurvivalPS->IsOnlyASpectator() && !SurvivalPS->IsInactive())
		{
			Result.Add(SurvivalPS);
		}
	}
	return Result;
}

void ACC_SurvivalGameState::SetPhase(ECC_MatchPhase NewPhase)
{
	if (!HasAuthority() || Phase == NewPhase)
	{
		return;
	}
	Phase = NewPhase;
	ForceNetUpdate();
	HandlePhaseChanged(); // 服务器收不到自己的 OnRep
}

void ACC_SurvivalGameState::OnRep_Phase(ECC_MatchPhase OldPhase)
{
	HandlePhaseChanged();
}

void ACC_SurvivalGameState::HandlePhaseChanged()
{
	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] Phase -> %s"), *UEnum::GetValueAsString(Phase));
	OnPhaseChanged.Broadcast(Phase);
	if (Phase == ECC_MatchPhase::GameOver && GetNetMode() != NM_DedicatedServer)
	{
		ResultScreenAttempts = 0;
		OpenResultScreenForLocalPlayers();
	}
}

void ACC_SurvivalGameState::OpenResultScreenForLocalPlayers()
{
	UWorld* World = GetWorld();
	if (!World || ResultScreenClass.IsNull())
	{
		return;
	}
	const TSubclassOf<UCC_ActivatableWidget> ScreenClass = ResultScreenClass.LoadSynchronous();
	if (!ScreenClass)
	{
		return;
	}
	bool bPending = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController())
		{
			continue;
		}
		UCC_RootLayout* Root = UCC_UIManagerSubsystem::GetRootLayoutForPlayer(Controller);
		if (!Root || !Root->CanAcceptScreen() || !Root->ShowScreen(ResultScreenLayer, ScreenClass, this))
		{
			bPending = true;
		}
	}
	// 根布局可能正在过渡（例如三选一刚被清空正在退场）。用核心 Ticker 重试，世界暂停时也会继续。
	if (bPending && ++ResultScreenAttempts <= 20 && !ResultScreenRetry.IsValid())
	{
		ResultScreenRetry = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			ResultScreenRetry.Reset();
			OpenResultScreenForLocalPlayers();
			return false; // 每次只跑一次；需要时 OpenResultScreenForLocalPlayers 会重新登记
		}), 0.25f);
	}
}

void ACC_SurvivalGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ResultScreenRetry.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ResultScreenRetry);
		ResultScreenRetry.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ACC_SurvivalGameState::AddTeamStat(FGameplayTag Tag, int32 Count)
{
	if (!HasAuthority() || Count == 0)
	{
		return;
	}
	TeamStats.AddStack(Tag, Count);
	ForceNetUpdate();
	OnTeamStatsChanged.Broadcast();
}

void ACC_SurvivalGameState::ResetTeamStats()
{
	if (!HasAuthority())
	{
		return;
	}
	TeamStats.Reset();
	OnTeamStatsChanged.Broadcast();
}

void ACC_SurvivalGameState::OnRep_TeamStats()
{
	OnTeamStatsChanged.Broadcast();
}

void ACC_SurvivalGameState::MulticastMatchMessage_Implementation(const FCC_MatchMessage& Message)
{
	OnMatchMessage.Broadcast(Message);
}

void ACC_SurvivalGameState::BroadcastMatchMessage(FGameplayTag Verb, APlayerState* InInstigator, APlayerState* Target, float Magnitude)
{
	if (!HasAuthority())
	{
		return;
	}
	FCC_MatchMessage Message;
	Message.Verb = Verb;
	Message.Instigator = InInstigator;
	Message.Target = Target;
	Message.Magnitude = Magnitude;
	MulticastMatchMessage(Message);
}
