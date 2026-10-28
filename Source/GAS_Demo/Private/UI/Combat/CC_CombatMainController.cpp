#include "UI/Combat/CC_CombatMainController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "Game/Survival/CC_TeamProgressionComponent.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CC_BuffSelectionComponent.h"

void UCC_CombatMainModel::SetPendingBuffRewards(int32 Value)
{
	if (PendingBuffRewards == Value) return;
	PendingBuffRewards = Value;
	NotifyChanged();
}

void UCC_CombatMainController::RefreshBuffRewards()
{
	const UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(GetPlayerController());
	if (UCC_CombatMainModel* CombatModel = Cast<UCC_CombatMainModel>(GetModel()))
	{
		CombatModel->SetPendingBuffRewards(Selection ? Selection->GetPendingOfferCount() : 0);
	}
}

bool UCC_CombatMainController::OpenBuffSelection()
{
	UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(GetPlayerController());
	return CanHandleActions() && Selection && Selection->RequestOpenSelection();
}

void UCC_CombatMainModel::SetVitals(const FCC_PlayerVitals& Value)
{
	if (Vitals == Value) return;
	Vitals = Value;
	NotifyChanged();
}

void UCC_CombatMainModel::SetSurvival(const FCC_SurvivalHUDSnapshot& Value)
{
	if (Survival == Value) return;
	Survival = Value;
	NotifyChanged();
}

UCC_CombatMainController::UCC_CombatMainController() { ModelClass = UCC_CombatMainModel::StaticClass(); }

void UCC_CombatMainController::OnActivated()
{
	if (APlayerController* Player = GetPlayerController())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		HandlePawnChanged(nullptr, Player->GetPawn());
	}
	Super::OnActivated();
	BindSurvival();
	if (UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(GetPlayerController()))
	{
		Selection->OnOfferChanged.AddUniqueDynamic(this, &ThisClass::RefreshBuffRewards);
	}
	RefreshBuffRewards();
}

void UCC_CombatMainController::OnDeactivated()
{
	if (UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(GetPlayerController()))
	{
		Selection->OnOfferChanged.RemoveDynamic(this, &ThisClass::RefreshBuffRewards);
	}
	if (APlayerController* Player = GetPlayerController()) Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	UnbindPawn();
	UnbindSurvival();
	Super::OnDeactivated();
}

// Pawn 变化时替换数据源，ASC 尚未就绪则等待通知。
void UCC_CombatMainController::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UnbindPawn();
	BoundCharacter = Cast<ACC_BaseCharacter>(NewPawn);
	if (BoundCharacter.IsValid()) BoundCharacter->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleASCReady);
	BindASC(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewPawn));
	// 客户端的 PlayerState 可能晚于界面激活到达，Pawn 变化时顺带补绑。
	if (IsActive()) BindSurvival();
}

// ---------------------------------------------------------------------------
// 生存模式数据（可选）
// ---------------------------------------------------------------------------

void UCC_CombatMainController::BindSurvival()
{
	UWorld* World = GetWorld();
	if (!World) return;
	ACC_SurvivalGameState* SurvivalState = World->GetGameState<ACC_SurvivalGameState>();
	if (!SurvivalState && !World->GetGameState())
	{
		// 客户端 GameState 还没复制到达：等它出现再绑。
		if (!GameStateSetHandle.IsValid())
		{
			GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		}
		return;
	}
	if (SurvivalState && BoundSurvivalState.Get() != SurvivalState)
	{
		BoundSurvivalState = SurvivalState;
		SurvivalState->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalPhaseChanged);
		SurvivalState->OnTeamStatsChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalChanged);
		SurvivalState->GetWaveDirector()->OnWaveStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalChanged);
		SurvivalState->GetProgression()->OnProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalChanged);
	}
	const APlayerController* Player = GetPlayerController();
	ACC_SurvivalPlayerState* SurvivalPlayer = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr;
	if (SurvivalPlayer != BoundSurvivalPlayer.Get())
	{
		if (ACC_SurvivalPlayerState* Old = BoundSurvivalPlayer.Get())
		{
			Old->OnSurvivalStateChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalChanged);
		}
		BoundSurvivalPlayer = SurvivalPlayer;
		if (SurvivalPlayer)
		{
			SurvivalPlayer->OnSurvivalStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalChanged);
		}
	}
	RefreshSurvival();
}

void UCC_CombatMainController::UnbindSurvival()
{
	if (UWorld* World = GetWorld(); World && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();
	if (ACC_SurvivalGameState* SurvivalState = BoundSurvivalState.Get())
	{
		SurvivalState->OnPhaseChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalPhaseChanged);
		SurvivalState->OnTeamStatsChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalChanged);
		SurvivalState->GetWaveDirector()->OnWaveStateChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalChanged);
		SurvivalState->GetProgression()->OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalChanged);
	}
	if (ACC_SurvivalPlayerState* SurvivalPlayer = BoundSurvivalPlayer.Get())
	{
		SurvivalPlayer->OnSurvivalStateChanged.RemoveDynamic(this, &ThisClass::HandleSurvivalChanged);
	}
	BoundSurvivalState.Reset();
	BoundSurvivalPlayer.Reset();
}

void UCC_CombatMainController::HandleGameStateSet(AGameStateBase* GameState)
{
	if (UWorld* World = GetWorld(); World && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();
	if (IsActive()) BindSurvival();
}

void UCC_CombatMainController::HandleSurvivalChanged() { RefreshSurvival(); }

void UCC_CombatMainController::HandleSurvivalPhaseChanged(ECC_MatchPhase NewPhase) { RefreshSurvival(); }

void UCC_CombatMainController::RefreshSurvival()
{
	if (!IsActive()) return;
	FCC_SurvivalHUDSnapshot Value;
	if (const ACC_SurvivalGameState* SurvivalState = BoundSurvivalState.Get())
	{
		const UCC_WaveDirectorComponent* Waves = SurvivalState->GetWaveDirector();
		const UCC_TeamProgressionComponent* Progress = SurvivalState->GetProgression();
		Value.bActive = true;
		Value.Phase = SurvivalState->GetPhase();
		Value.CurrentWave = Waves->GetCurrentWave();
		Value.NextWaveServerTime = Waves->GetNextWaveServerTime();
		Value.AliveEnemies = Waves->GetAliveEnemyCount();
		Value.TeamLevel = Progress->GetTeamLevel();
		Value.TeamExperience = Progress->GetTeamExperience();
		Value.ExperienceToNextLevel = Progress->GetExperienceToNextLevel();
		Value.TeamKills = SurvivalState->GetTeamStat(CCSurvivalTags::StatKills);
	}
	if (const ACC_SurvivalPlayerState* SurvivalPlayer = BoundSurvivalPlayer.Get())
	{
		Value.MyKills = SurvivalPlayer->GetStat(CCSurvivalTags::StatKills);
		Value.LifeState = SurvivalPlayer->GetLifeState();
		Value.RespawnServerTime = SurvivalPlayer->GetRespawnServerTime();
	}
	if (UCC_CombatMainModel* CombatModel = Cast<UCC_CombatMainModel>(GetModel()))
	{
		CombatModel->SetSurvival(Value);
	}
}
// 角色延迟初始化完成后，把战斗主界面绑定到新的能力系统。
void UCC_CombatMainController::HandleASCReady(UAbilitySystemComponent* NewASC, UAttributeSet* Attributes) { BindASC(NewASC); }
// 同时订阅当前值和最大值，绑定后主动推送初始快照。
void UCC_CombatMainController::BindASC(UAbilitySystemComponent* NewASC)
{
	UnbindASC();
	BoundASC = NewASC;
	if (NewASC)
	{
		for (FGameplayAttribute Attribute :{
				UCC_AttributeSet::GetHealthAttribute(),
				UCC_AttributeSet::GetMaxHealthAttribute(),
				UCC_AttributeSet::GetManaAttribute(),
				UCC_AttributeSet::GetMaxManaAttribute()})
		{
			AttributeHandles.Add(Attribute, NewASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
		}
	}
	Refresh();
}
// 按句柄移除本界面的订阅，不影响其他观察者。
void UCC_CombatMainController::UnbindASC()
{
	if (BoundASC.IsValid()) for (const auto& Pair : AttributeHandles) BoundASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
	AttributeHandles.Reset();
	BoundASC.Reset();
}
// 一并解除角色初始化事件和属性事件。
void UCC_CombatMainController::UnbindPawn()
{
	if (BoundCharacter.IsValid()) BoundCharacter->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleASCReady);
	BoundCharacter.Reset();
	UnbindASC();
}
// 任意一项属性变化都刷新整组展示，保持当前值和最大值一致。
void UCC_CombatMainController::HandleAttributeChanged(const FOnAttributeChangeData& Data) { Refresh(); }
// 读取当前快照、安全计算进度比例，并通知蓝图展示层。
void UCC_CombatMainController::Refresh()
{
	if (!IsActive()) return;
	// 缺少数据源时发布零值，角色切换期间不保留旧角色数值。
	const auto Read = [this](FGameplayAttribute Attribute) { return BoundASC.IsValid() ? BoundASC->GetNumericAttribute(Attribute) : 0.f; };
	FCC_PlayerVitals Value;
	Value.Health = Read(UCC_AttributeSet::GetHealthAttribute());
	Value.MaxHealth = Read(UCC_AttributeSet::GetMaxHealthAttribute());
	Value.Mana = Read(UCC_AttributeSet::GetManaAttribute());
	Value.MaxMana = Read(UCC_AttributeSet::GetMaxManaAttribute());
	if (UCC_CombatMainModel* CombatModel = Cast<UCC_CombatMainModel>(GetModel()))
	{
		CombatModel->SetVitals(Value);
	}
		
}
