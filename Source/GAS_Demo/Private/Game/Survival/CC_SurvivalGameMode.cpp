#include "Game/Survival/CC_SurvivalGameMode.h"

#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Character/CC_PlayerCharacter.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CC_BuffSelectionComponent.h"
#include "Damage/CC_DamageTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Survival/CC_RespawnComponent.h"
#include "Game/Survival/CC_SurvivalCVars.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalModeConfig.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "Game/Survival/CC_TeamProgressionComponent.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS_Demo.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/CC_GameEventSubsystem.h"
#include "TimerManager.h"
#include "Utils/Team/CC_TeamStatics.h"

#pragma region 生命周期与初始化

ACC_SurvivalGameMode::ACC_SurvivalGameMode()
{
	GameStateClass = ACC_SurvivalGameState::StaticClass();
	PlayerStateClass = ACC_SurvivalPlayerState::StaticClass();
	Respawn = CreateDefaultSubobject<UCC_RespawnComponent>(TEXT("Respawn"));
}


void ACC_SurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString ConfigPath = UGameplayStatics::ParseOption(Options, TEXT("SurvivalConfig"));
	if (!ConfigPath.IsEmpty())
	{
		if (UCC_SurvivalModeConfig* Override = LoadObject<UCC_SurvivalModeConfig>(nullptr, *ConfigPath))
		{
			Config = Override;
		}
		else
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] ?SurvivalConfig=%s 加载失败，使用默认配置"), *ConfigPath);
		}
	}
	const FString SeedOption = UGameplayStatics::ParseOption(Options, TEXT("SurvivalSeed"));
	bSeedFromOptions = !SeedOption.IsEmpty();
	Seed = bSeedFromOptions ? FCString::Atoi(*SeedOption) : FMath::Rand();
}

void ACC_SurvivalGameMode::StartPlay()
{
	Super::StartPlay();
	if (!Config)
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("[Survival] %s 没有配置 Config（UCC_SurvivalModeConfig），生存模式不会开始。"), *GetName());
		return;
	}
	Respawn->SetConfig(Config);
	if (UCC_GameEventSubsystem* Events = UCC_GameEventSubsystem::Get(this))
	{
		DiedHandle = Events->OnCharacterDied.AddUObject(this, &ThisClass::HandleCharacterDied);
		DeathFinishedHandle = Events->OnCharacterDeathFinished.AddUObject(this, &ThisClass::HandleCharacterDeathFinished);
	}
	WaitStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(StartCheckTimer, this, &ThisClass::TryStartMatch, 0.5f, true, 0.1f);
#if !UE_BUILD_SHIPPING
	GetWorldTimerManager().SetTimer(DebugDisplayTimer, this, &ThisClass::UpdateDebugDisplay, 0.25f, true);
#endif
}

void ACC_SurvivalGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCC_GameEventSubsystem* Events = UCC_GameEventSubsystem::Get(this))
	{
		Events->OnCharacterDied.Remove(DiedHandle);
		Events->OnCharacterDeathFinished.Remove(DeathFinishedHandle);
	}
	DiedHandle.Reset();
	DeathFinishedHandle.Reset();
	UnbindAllPlayers();
	GetWorldTimerManager().ClearTimer(StartCheckTimer);
	GetWorldTimerManager().ClearTimer(DebugDisplayTimer);
	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region 对局状态查询

ACC_SurvivalGameState* ACC_SurvivalGameMode::GetSurvivalGameState() const
{
	return GetGameState<ACC_SurvivalGameState>();
}

bool ACC_SurvivalGameMode::IsMatchPlaying() const
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	return SurvivalState && SurvivalState->GetPhase() == ECC_MatchPhase::Playing;
}

#pragma endregion

#pragma region 对局开始与结束

bool ACC_SurvivalGameMode::AreAllPlayersReady() const
{
	int32 Players = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		if (!Player || !Player->PlayerState || Player->PlayerState->IsOnlyASpectator())
		{
			continue;
		}
		const ACC_PlayerCharacter* Pawn = Cast<ACC_PlayerCharacter>(Player->GetPawn());
		if (!Pawn || !Pawn->IsAlive())
		{
			return false;
		}
		++Players;
	}
	return Players > 0;
}

void ACC_SurvivalGameMode::TryStartMatch()
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState || SurvivalState->GetPhase() != ECC_MatchPhase::WaitingToStart)
	{
		GetWorldTimerManager().ClearTimer(StartCheckTimer);
		return;
	}
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - WaitStartTime >= StartTimeout && GetNumPlayers() > 0;
	if (AreAllPlayersReady() || bTimedOut)
	{
		GetWorldTimerManager().ClearTimer(StartCheckTimer);
		StartMatch();
	}
}

void ACC_SurvivalGameMode::StartMatch()
{
	ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!Config || !SurvivalState || SurvivalState->GetPhase() != ECC_MatchPhase::WaitingToStart)
	{
		return;
	}
	SurvivalState->ResetTeamStats();
	for (ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
	{
		PS->ResetForMatch();
	}
	SurvivalState->MatchStartServerTime = SurvivalState->GetServerWorldTimeSeconds();
	SurvivalState->MatchEndServerTime = 0.0;
	SurvivalState->SetPhase(ECC_MatchPhase::Playing);
	SurvivalState->GetProgression()->StartProgression(Config);
	SurvivalState->GetWaveDirector()->StartDirector(Config, Seed);
	SurvivalState->BroadcastMatchMessage(CCSurvivalTags::MsgMatchStarted);
	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 对局开始，玩家 %d，种子 %d%s（复现：?SurvivalSeed=%d）"),
		SurvivalState->GetSurvivalPlayers().Num(), Seed, bSeedFromOptions ? TEXT("（来自 URL）") : TEXT(""), Seed);
}

void ACC_SurvivalGameMode::EndMatch()
{
	ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState || SurvivalState->GetPhase() != ECC_MatchPhase::Playing)
	{
		return;
	}
	// 顺序见 Docs/SurvivalMode.zh-CN.md 6.1
	SurvivalState->GetWaveDirector()->StopDirector();
	SurvivalState->GetProgression()->StopProgression();
	Respawn->CancelAll();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(It->Get()))
		{
			Selection->ClearOffers(); // 也会经由 OnServerPendingChanged 撤掉选卡保护
		}
	}
	FreezeEnemies();
	SurvivalState->MatchEndServerTime = SurvivalState->GetServerWorldTimeSeconds();
	UnbindAllPlayers();
	SurvivalState->BroadcastMatchMessage(CCSurvivalTags::MsgMatchEnded);
	SurvivalState->SetPhase(ECC_MatchPhase::GameOver);
	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 对局结束：第 %d 波，等级 %d，全队击杀 %d，用时 %.0f 秒"),
		SurvivalState->GetWaveDirector()->GetCurrentWave(), SurvivalState->GetProgression()->GetTeamLevel(),
		SurvivalState->GetTeamStat(CCSurvivalTags::StatKills), SurvivalState->GetMatchElapsedSeconds());
}

void ACC_SurvivalGameMode::FreezeEnemies()
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState)
	{
		return;
	}
	for (ACC_EnemyCharacter* Enemy : SurvivalState->GetWaveDirector()->GetAliveEnemies())
	{
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Survival GameOver"));
			}
		}
	}
}

#pragma endregion

#pragma region 重开与返回大厅

void ACC_SurvivalGameMode::RequestPlayAgain()
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState || SurvivalState->GetPhase() != ECC_MatchPhase::GameOver)
	{
		return;
	}
	// 重新加载当前地图；bUseSeamlessTravel（房间基类开启）时为无缝切图，PlayerState 会重新创建。
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}

void ACC_SurvivalGameMode::RequestReturnToLobby()
{
	if (!ReturnToLobby())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] 返回大厅失败（单机或没有房间信息）"));
	}
}

#pragma endregion

#pragma region 玩家加入与离开

void ACC_SurvivalGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	// 房间基类可能因满员销毁连接。
	if (!IsValid(NewPlayer) || NewPlayer->IsActorBeingDestroyed())
	{
		return;
	}
	BindPlayer(NewPlayer);

	if (IsMatchPlaying())
	{
		// 中途加入（房主允许时才会到这里）：在队友身边出生，补发三选一。
		if (ACC_SurvivalPlayerState* PS = NewPlayer->GetPlayerState<ACC_SurvivalPlayerState>())
		{
			PS->ResetForMatch();
		}
		PlaceNearTeammate(NewPlayer);
		if (ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
		{
			SurvivalState->GetProgression()->GrantCatchUpOffers(NewPlayer);
		}
	}
}

void ACC_SurvivalGameMode::Logout(AController* Exiting)
{
	if (ACC_SurvivalPlayerState* PS = Exiting ? Exiting->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr)
	{
		Respawn->Cancel(PS);
		UnbindPlayer(PS);
	}
	Super::Logout(Exiting);
	CheckTeamWipedNextTick();
}

void ACC_SurvivalGameMode::PlaceNearTeammate(APlayerController* Player)
{
	ACC_PlayerCharacter* Pawn = Player ? Cast<ACC_PlayerCharacter>(Player->GetPawn()) : nullptr;
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!Pawn || !SurvivalState)
	{
		return;
	}
	for (const ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
	{
		const ACC_PlayerCharacter* Anchor = Cast<ACC_PlayerCharacter>(PS->GetPawn());
		if (Anchor && Anchor != Pawn && PS->IsAliveInMatch() && Anchor->IsAlive())
		{
			Pawn->TeleportTo(Respawn->FindReviveLocation(Anchor, Pawn), Pawn->GetActorRotation(), false, true);
			return;
		}
	}
}

#pragma endregion

#pragma region 玩家事件订阅与清理

void ACC_SurvivalGameMode::BindPlayer(APlayerController* Player)
{
	ACC_SurvivalPlayerState* PS = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	UnbindPlayer(PS);
	const TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPS(PS);
	FPlayerBinding& Binding = Bindings.Add(PS);

	if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
	{
		Binding.ASC = ASC;
		Binding.DamageDealtHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(FGameplayTag(CCDamageTags::EventDamageDealt))
			.AddUObject(this, &ThisClass::HandleDamageEvent, WeakPS, true);
		Binding.DamageTakenHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(FGameplayTag(CCDamageTags::EventDamageTaken))
			.AddUObject(this, &ThisClass::HandleDamageEvent, WeakPS, false);
	}
	if (UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(Player))
	{
		Binding.Selection = Selection;
		Binding.PendingHandle = Selection->OnServerPendingChanged.AddUObject(this, &ThisClass::HandleOfferPendingChanged, WeakPS);
		Binding.AcceptedHandle = Selection->OnServerChoiceAccepted.AddUObject(this, &ThisClass::HandleChoiceAccepted, WeakPS);
		if (Selection->HasPendingOffer())
		{
			HandleOfferPendingChanged(true, WeakPS);
		}
	}
}

void ACC_SurvivalGameMode::UnbindPlayer(ACC_SurvivalPlayerState* PlayerState)
{
	FPlayerBinding* Binding = PlayerState ? Bindings.Find(PlayerState) : nullptr;
	if (!Binding)
	{
		return;
	}
	SetChoosingProtection(PlayerState, false);
	if (UAbilitySystemComponent* ASC = Binding->ASC.Get())
	{
		if (FGameplayEventMulticastDelegate* Dealt = ASC->GenericGameplayEventCallbacks.Find(FGameplayTag(CCDamageTags::EventDamageDealt)))
		{
			Dealt->Remove(Binding->DamageDealtHandle);
		}
		if (FGameplayEventMulticastDelegate* Taken = ASC->GenericGameplayEventCallbacks.Find(FGameplayTag(CCDamageTags::EventDamageTaken)))
		{
			Taken->Remove(Binding->DamageTakenHandle);
		}
	}
	if (UCC_BuffSelectionComponent* Selection = Binding->Selection.Get())
	{
		Selection->OnServerPendingChanged.Remove(Binding->PendingHandle);
		Selection->OnServerChoiceAccepted.Remove(Binding->AcceptedHandle);
	}
	Bindings.Remove(PlayerState);
}

void ACC_SurvivalGameMode::UnbindAllPlayers()
{
	TArray<TWeakObjectPtr<ACC_SurvivalPlayerState>> Keys;
	Bindings.GetKeys(Keys);
	for (const TWeakObjectPtr<ACC_SurvivalPlayerState>& Key : Keys)
	{
		UnbindPlayer(Key.Get());
	}
	// 已销毁的 PlayerState 无法再退订（其 ASC 也随之销毁），直接丢弃记录。
	Bindings.Reset();
}

#pragma endregion

#pragma region 伤害统计

void ACC_SurvivalGameMode::HandleDamageEvent(const FGameplayEventData* Payload,
	TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState, bool bDealt)
{
	ACC_SurvivalPlayerState* PS = WeakPlayerState.Get();
	if (!PS || !Payload || !IsMatchPlaying())
	{
		return;
	}
	const int32 Amount = FMath::RoundToInt(Payload->EventMagnitude);
	if (Amount <= 0)
	{
		return;
	}
	PS->AddStat(bDealt ? CCSurvivalTags::StatDamageDealt : CCSurvivalTags::StatDamageTaken, Amount);
	if (bDealt)
	{
		if (ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
		{
			SurvivalState->AddTeamStat(CCSurvivalTags::StatDamageDealt, Amount);
		}
	}
}

#pragma endregion

#pragma region 选卡统计与保护

void ACC_SurvivalGameMode::HandleOfferPendingChanged(bool bHasPending, TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState)
{
	// 单机打开三选一会暂停世界，不需要保护。
	const bool bEnable = bHasPending && IsMatchPlaying() && GetNetMode() != NM_Standalone;
	SetChoosingProtection(WeakPlayerState.Get(), bEnable);
}

void ACC_SurvivalGameMode::HandleChoiceAccepted(const FCC_BuffOfferOption& Option, TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState)
{
	if (ACC_SurvivalPlayerState* PS = WeakPlayerState.Get())
	{
		PS->AddStat(CCSurvivalTags::StatBuffsChosen, 1);
	}
}

void ACC_SurvivalGameMode::SetChoosingProtection(ACC_SurvivalPlayerState* PlayerState, bool bEnable)
{
	FPlayerBinding* Binding = PlayerState ? Bindings.Find(PlayerState) : nullptr;
	UAbilitySystemComponent* ASC = Binding ? Binding->ASC.Get() : nullptr;
	if (!ASC)
	{
		return;
	}
	if (!bEnable)
	{
		if (Binding->ProtectionHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Binding->ProtectionHandle);
			Binding->ProtectionHandle.Invalidate();
		}
		return;
	}
	if (Binding->ProtectionHandle.IsValid() || !Config || !Config->Progression.ChoosingProtectionEffect)
	{
		return;
	}
	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Config->Progression.ChoosingProtectionEffect, 1.f, Context);
	if (Spec.IsValid())
	{
		Binding->ProtectionHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

#pragma endregion

#pragma region 死亡事件与击杀归属

/**
 * 赛选，区分玩家角色死亡 和 怪物死亡
 * @param Info 
 */
void ACC_SurvivalGameMode::HandleCharacterDied(const FCC_DeathInfo& Info)
{
	// GameOver 之后到达的死亡（DoT、飞行中的箭）一律忽略。
	if (!IsMatchPlaying() || !Info.Victim)
	{
		return;
	}
	if (ACC_PlayerCharacter* Player = Cast<ACC_PlayerCharacter>(Info.Victim))
	{
		if (ACC_SurvivalPlayerState* PS = Player->GetPlayerState<ACC_SurvivalPlayerState>())
		{
			HandlePlayerDied(PS);
		}
		return;
	}
	if (Cast<ACC_EnemyCharacter>(Info.Victim))
	{
		HandleEnemyDied(Info);
	}
}

bool ACC_SurvivalGameMode::ShouldCreditKill(const FCC_DeathInfo& Info) const
{
	if (!Info.Killer || !Info.Victim || !UCC_TeamStatics::IsHostile(Info.Killer, Info.Victim))
	{
		return false;
	}
	if (Info.KillerPlayerState)
	{
		return true;
	}
	// 非玩家击杀者（被魅惑的怪、以后的召唤物）：与任一玩家同阵营即算玩家方。只走统一敌对查询（硬性规则 4）。
	if (const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
	{
		for (const ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
		{
			const APawn* PlayerPawn = PS->GetPawn();
			if (PlayerPawn && PlayerPawn != Info.Killer && !UCC_TeamStatics::IsHostile(Info.Killer, PlayerPawn)
				&& UCC_TeamStatics::IsHostile(PlayerPawn, Info.Victim))
			{
				return true;
			}
		}
	}
	return false;
}

void ACC_SurvivalGameMode::HandleEnemyDied(const FCC_DeathInfo& Info)
{
	ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	ACC_EnemyCharacter* Enemy = Cast<ACC_EnemyCharacter>(Info.Victim);
	if (!SurvivalState || !Enemy)
	{
		return;
	}
	SurvivalState->GetWaveDirector()->NotifyEnemyRemoved(Enemy);
	if (!ShouldCreditKill(Info))
	{
		return;
	}
	if (ACC_SurvivalPlayerState* KillerPS = Cast<ACC_SurvivalPlayerState>(Info.KillerPlayerState))
	{
		KillerPS->AddStat(CCSurvivalTags::StatKills, 1);
	}
	SurvivalState->AddTeamStat(CCSurvivalTags::StatKills, 1);
	SurvivalState->GetProgression()->AddExperience(Enemy->GetExperienceReward() * CCSurvivalCVars::GetExperienceScale());
}

#pragma endregion

#pragma region 玩家阵亡复活与观战

void ACC_SurvivalGameMode::HandlePlayerDied(ACC_SurvivalPlayerState* PlayerState)
{
	ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState || !PlayerState->IsAliveInMatch())
	{
		return;
	}
	PlayerState->AddStat(CCSurvivalTags::StatDeaths, 1);
	SurvivalState->AddTeamStat(CCSurvivalTags::StatDeaths, 1);
	PlayerState->SetLifeState(ECC_PlayerLifeState::Dying);
	SurvivalState->BroadcastMatchMessage(CCSurvivalTags::MsgPlayerDown, nullptr, PlayerState);
	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] %s 阵亡（第 %d 次）"), *PlayerState->GetPlayerName(),
		PlayerState->GetStat(CCSurvivalTags::StatDeaths));

	if (IsTeamWiped())
	{
		EndMatch();
		return;
	}
	Respawn->SchedulePlayer(PlayerState);
}

void ACC_SurvivalGameMode::HandleCharacterDeathFinished(ACC_BaseCharacter* Character)
{
	const ACC_PlayerCharacter* Player = Cast<ACC_PlayerCharacter>(Character);
	ACC_SurvivalPlayerState* PS = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr;
	if (!PS || !IsMatchPlaying() || PS->GetLifeState() != ECC_PlayerLifeState::Dying)
	{
		return;
	}
	PS->SetLifeState(ECC_PlayerLifeState::Dead);
	SpectateTeammate(PS);
	Respawn->NotifyDeathFinished(PS);
}

void ACC_SurvivalGameMode::HandlePlayerRevived(ACC_SurvivalPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}
	PlayerState->SetLifeState(ECC_PlayerLifeState::Alive);
	PlayerState->SetRespawnServerTime(0.0);
	PlayerState->AddStat(CCSurvivalTags::StatRevives, 1);
	if (APlayerController* Player = PlayerState->GetPlayerController())
	{
		// 服务器设置视角会自动同步给远端客户端。
		Player->SetViewTargetWithBlend(Player->GetPawn(), 0.3f);
	}
	if (ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
	{
		SurvivalState->BroadcastMatchMessage(CCSurvivalTags::MsgPlayerRevived, nullptr, PlayerState);
	}
}

void ACC_SurvivalGameMode::SpectateTeammate(ACC_SurvivalPlayerState* PlayerState)
{
	APlayerController* Player = PlayerState->GetPlayerController();
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!Player || !SurvivalState)
	{
		return;
	}
	for (const ACC_SurvivalPlayerState* Other : SurvivalState->GetSurvivalPlayers())
	{
		APawn* OtherPawn = Other != PlayerState ? Other->GetPawn() : nullptr;
		if (OtherPawn && Other->IsAliveInMatch())
		{
			Player->SetViewTargetWithBlend(OtherPawn, 0.5f);
			return;
		}
	}
}

#pragma endregion

#pragma region 全队阵亡判定

bool ACC_SurvivalGameMode::IsTeamWiped() const
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState)
	{
		return false;
	}
	for (const ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
	{
		// 已断线（没有控制器）的不算存活。
		if (PS->IsAliveInMatch() && PS->GetPlayerController())
		{
			return false;
		}
	}
	return true;
}

void ACC_SurvivalGameMode::CheckTeamWipedNextTick()
{
	// Logout 时 PlayerState 还在列表里，下一帧再判断。
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsMatchPlaying() && IsTeamWiped())
		{
			EndMatch();
		}
	}));
}

#pragma endregion

#pragma region 调试功能

void ACC_SurvivalGameMode::DebugAddExperience(float Amount)
{
	if (ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
	{
		SurvivalState->GetProgression()->AddExperience(Amount);
	}
}

void ACC_SurvivalGameMode::DebugReviveAll()
{
	if (const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState())
	{
		for (ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
		{
			if (!PS->IsAliveInMatch())
			{
				Respawn->ReviveNow(PS);
			}
		}
	}
}

void ACC_SurvivalGameMode::DebugToggleGodMode()
{
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!SurvivalState)
	{
		return;
	}
	for (ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			// 复用伤害 GE 的通用免疫标签（Status.DamageImmunity），不新增伤害判断。
			if (ASC->GetTagCount(CCDamageTags::Immunity) > 0)
			{
				ASC->RemoveLooseGameplayTag(CCDamageTags::Immunity);
			}
			else
			{
				ASC->AddLooseGameplayTag(CCDamageTags::Immunity);
			}
			UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] %s 无敌：%s"), *PS->GetPlayerName(),
				ASC->GetTagCount(CCDamageTags::Immunity) > 0 ? TEXT("开") : TEXT("关"));
		}
	}
}

void ACC_SurvivalGameMode::UpdateDebugDisplay()
{
#if !UE_BUILD_SHIPPING
	const ACC_SurvivalGameState* SurvivalState = GetSurvivalGameState();
	if (!GEngine || !SurvivalState || !CCSurvivalCVars::IsDebugDisplayEnabled())
	{
		return;
	}
	const UCC_WaveDirectorComponent* Waves = SurvivalState->GetWaveDirector();
	const UCC_TeamProgressionComponent* Progress = SurvivalState->GetProgression();
	FString Text = FString::Printf(TEXT("[Survival] %s  波次 %d  下一波 %.1fs  存活 %d  队列 %d\n等级 %d  经验 %.0f/%.0f  全队击杀 %d"),
		*UEnum::GetValueAsString(SurvivalState->GetPhase()), Waves->GetCurrentWave(),
		SurvivalState->GetSecondsUntilServerTime(Waves->GetNextWaveServerTime()), Waves->GetAliveEnemyCount(),
		Waves->GetQueuedSpawnCount(), Progress->GetTeamLevel(), Progress->GetTeamExperience(),
		Progress->GetExperienceToNextLevel(), SurvivalState->GetTeamStat(CCSurvivalTags::StatKills));
	for (const ACC_SurvivalPlayerState* PS : SurvivalState->GetSurvivalPlayers())
	{
		Text += FString::Printf(TEXT("\n  %s  %s  击杀 %d  阵亡 %d  复活倒计时 %.1fs"), *PS->GetPlayerName(),
			*UEnum::GetValueAsString(PS->GetLifeState()), PS->GetStat(CCSurvivalTags::StatKills),
			PS->GetStat(CCSurvivalTags::StatDeaths), PS->GetRespawnRemainingSeconds());
	}
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.3f, FColor::Cyan, Text);
#endif
}

#pragma endregion
