#include "UI/Toast/CC_SurvivalToastRelay.h"
#include "UI/Toast/CC_ToastSettings.h"
#include "UI/Toast/CC_ToastSubsystem.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "Game/Survival/CC_SurvivalTypes.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "CCToast"

bool UCC_SurvivalToastRelay::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer() || !Super::ShouldCreateSubsystem(Outer)) return false;
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UCC_SurvivalToastRelay::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// 客户端的 GameState 是复制来的，可能晚于 BeginPlay；两条路都接上。
	GameStateSetHandle = InWorld.GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	HandleGameStateSet(InWorld.GetGameState());
}

void UCC_SurvivalToastRelay::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
		World->GetTimerManager().ClearTimer(WaveIncomingTimer);
	}
	Unbind();
	Super::Deinitialize();
}

void UCC_SurvivalToastRelay::HandleGameStateSet(AGameStateBase* GameState)
{
	ACC_SurvivalGameState* Survival = Cast<ACC_SurvivalGameState>(GameState);
	if (Survival && Survival != BoundGameState.Get())
	{
		Unbind();
		Bind(Survival);
	}
}

void UCC_SurvivalToastRelay::Bind(ACC_SurvivalGameState* GameState)
{
	BoundGameState = GameState;
	GameState->OnMatchMessage.AddUniqueDynamic(this, &ThisClass::HandleMatchMessage);
	if (UCC_WaveDirectorComponent* Waves = GameState->GetWaveDirector())
	{
		Waves->OnWaveStateChanged.AddUniqueDynamic(this, &ThisClass::HandleWaveStateChanged);
	}
	HandleWaveStateChanged();
}

void UCC_SurvivalToastRelay::Unbind()
{
	if (ACC_SurvivalGameState* GameState = BoundGameState.Get())
	{
		GameState->OnMatchMessage.RemoveDynamic(this, &ThisClass::HandleMatchMessage);
		if (UCC_WaveDirectorComponent* Waves = GameState->GetWaveDirector())
		{
			Waves->OnWaveStateChanged.RemoveDynamic(this, &ThisClass::HandleWaveStateChanged);
		}
	}
	BoundGameState.Reset();
	ScheduledForServerTime = -1.0;
}

bool UCC_SurvivalToastRelay::GetBuiltinTemplate(FGameplayTag Tag, FCC_ToastTemplate& Out)
{
	Out = FCC_ToastTemplate();
	if (Tag == CCSurvivalTags::MsgMatchStarted)
	{
		Out.Title = LOCTEXT("MatchStarted", "生存开始");
		Out.Message = LOCTEXT("MatchStartedMsg", "坚持下去，活得越久越强");
		Out.Style = ECC_ToastStyle::Info;
		Out.Priority = 10;
		return true;
	}
	if (Tag == CCSurvivalTags::MsgWaveStarted)
	{
		Out.Title = LOCTEXT("WaveStarted", "第 {Value} 波");
		Out.Message = LOCTEXT("WaveStartedMsg", "敌人来袭！");
		Out.Style = ECC_ToastStyle::Warning;
		Out.Priority = 20;
		Out.DedupeKey = TEXT("Survival.Wave");
		return true;
	}
	if (Tag == CCSurvivalTags::MsgLevelUp)
	{
		Out.Title = LOCTEXT("LevelUp", "全队升级");
		Out.Message = LOCTEXT("LevelUpMsg", "等级提升至 Lv.{Value}");
		Out.Style = ECC_ToastStyle::Success;
		Out.Priority = 5;
		Out.DedupeKey = TEXT("Survival.LevelUp");
		return true;
	}
	if (Tag == CCSurvivalTags::MsgPlayerDown)
	{
		Out.Title = LOCTEXT("PlayerDown", "{Player} 倒下了");
		Out.Message = LOCTEXT("PlayerDownMsg", "稍后将自动复活");
		Out.Style = ECC_ToastStyle::Danger;
		Out.TargetFilter = ECC_ToastTargetFilter::OnlyOthers;
		return true;
	}
	if (Tag == CCSurvivalTags::MsgPlayerRevived)
	{
		Out.Title = LOCTEXT("PlayerRevived", "{Player} 已复活");
		Out.Style = ECC_ToastStyle::Success;
		Out.TargetFilter = ECC_ToastTargetFilter::OnlyOthers;
		return true;
	}
	if (Tag == CCToastTags::WaveIncoming)
	{
		Out.Title = LOCTEXT("WaveIncoming", "敌人即将来袭");
		Out.Message = LOCTEXT("WaveIncomingMsg", "{Value} 秒后第 {Wave} 波开始");
		Out.Style = ECC_ToastStyle::Warning;
		Out.Priority = 1;
		Out.DedupeKey = TEXT("Survival.WaveIncoming");
		return true;
	}
	// MatchEnded：结算页会打开，默认不弹提示。
	return false;
}

bool UCC_SurvivalToastRelay::ResolveTemplate(FGameplayTag Tag, FCC_ToastTemplate& OutTemplate)
{
	const UCC_ToastSettings* Settings = GetDefault<UCC_ToastSettings>();
	if (const FCC_ToastTemplate* Configured = Settings->MessageToasts.Find(Tag))
	{
		OutTemplate = *Configured;
		return OutTemplate.bEnabled;
	}
	return Settings->bUseBuiltinMessageToasts && GetBuiltinTemplate(Tag, OutTemplate) && OutTemplate.bEnabled;
}

void UCC_SurvivalToastRelay::HandleMatchMessage(const FCC_MatchMessage& Message)
{
	FCC_ToastTemplate Template;
	if (!ResolveTemplate(Message.Verb, Template)) return;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Value"), FText::AsNumber(FMath::RoundToInt(Message.Magnitude)));
	const APlayerState* Named = Message.Target ? Message.Target.Get() : Message.Instigator.Get();
	Args.Add(TEXT("Player"), Named ? FText::FromString(Named->GetPlayerName()) : LOCTEXT("Teammate", "队友"));
	Args.Add(TEXT("Wave"), FText::AsNumber(FMath::RoundToInt(Message.Magnitude)));
	Dispatch(Message.Verb, Template, Args, Message.Target.Get());
}

void UCC_SurvivalToastRelay::Dispatch(FGameplayTag Tag, const FCC_ToastTemplate& Template, const FFormatNamedArguments& Args, const APlayerState* Target) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance) return;
	const FCC_ToastRequest Request = Template.ToRequest(Args, Tag);
	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		if (!LocalPlayer) continue;
		const APlayerController* PC = LocalPlayer->PlayerController;
		const bool bIsSelf = Target && PC && PC->PlayerState == Target;
		if (Template.TargetFilter == ECC_ToastTargetFilter::OnlyOthers && bIsSelf) continue;
		if (Template.TargetFilter == ECC_ToastTargetFilter::OnlySelf && !bIsSelf) continue;
		if (UCC_ToastSubsystem* Toasts = LocalPlayer->GetSubsystem<UCC_ToastSubsystem>())
		{
			Toasts->ShowToast(Request);
		}
	}
}

void UCC_SurvivalToastRelay::HandleWaveStateChanged()
{
	UWorld* World = GetWorld();
	ACC_SurvivalGameState* GameState = BoundGameState.Get();
	const UCC_WaveDirectorComponent* Waves = GameState ? GameState->GetWaveDirector() : nullptr;
	const float Lead = GetDefault<UCC_ToastSettings>()->WaveIncomingLeadSeconds;
	if (!World || !Waves || Lead <= 0.f) return;

	const double NextTime = Waves->GetNextWaveServerTime();
	if (NextTime <= 0.0 || GameState->GetPhase() != ECC_MatchPhase::Playing)
	{
		World->GetTimerManager().ClearTimer(WaveIncomingTimer);
		ScheduledForServerTime = -1.0;
		return;
	}
	if (FMath::IsNearlyEqual(NextTime, ScheduledForServerTime)) return;

	ScheduledForServerTime = NextTime;
	ScheduledWave = Waves->GetCurrentWave() + 1;
	const float Remaining = GameState->GetSecondsUntilServerTime(NextTime);
	// 剩余时间不足预告时长（例如中途加入）时不弹，避免数字对不上。
	const float Delay = Remaining - Lead;
	World->GetTimerManager().ClearTimer(WaveIncomingTimer);
	if (Delay > 0.05f)
	{
		World->GetTimerManager().SetTimer(WaveIncomingTimer, FTimerDelegate::CreateUObject(this, &ThisClass::FireWaveIncoming), Delay, false);
	}
}

void UCC_SurvivalToastRelay::FireWaveIncoming()
{
	const ACC_SurvivalGameState* GameState = BoundGameState.Get();
	FCC_ToastTemplate Template;
	if (!GameState || GameState->GetPhase() != ECC_MatchPhase::Playing || !ResolveTemplate(CCToastTags::WaveIncoming, Template)) return;

	const int32 Seconds = FMath::Max(1, FMath::RoundToInt(GameState->GetSecondsUntilServerTime(ScheduledForServerTime)));
	FFormatNamedArguments Args;
	Args.Add(TEXT("Value"), FText::AsNumber(Seconds));
	Args.Add(TEXT("Wave"), FText::AsNumber(ScheduledWave));
	Args.Add(TEXT("Player"), FText::GetEmpty());
	Dispatch(CCToastTags::WaveIncoming, Template, Args, nullptr);
}

#undef LOCTEXT_NAMESPACE
