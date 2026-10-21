#include "Game/Survival/CC_WaveDirectorComponent.h"

#include "AbilitySystemGlobals.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Spawners/CC_EnemySpawner.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Survival/CC_SurvivalCVars.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalModeConfig.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "GameplayCueManager.h"
#include "GAS_Demo.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	constexpr float CleanupInterval = 2.f;
}

UCC_WaveDirectorComponent::UCC_WaveDirectorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UCC_WaveDirectorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCC_WaveDirectorComponent, CurrentWave);
	DOREPLIFETIME(UCC_WaveDirectorComponent, NextWaveServerTime);
	DOREPLIFETIME(UCC_WaveDirectorComponent, AliveEnemyCount);
}

void UCC_WaveDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDirector();
	PreloadHandles.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// 启停
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::StartDirector(const UCC_SurvivalModeConfig* InConfig, int32 Seed)
{
	if (!GetOwner()->HasAuthority() || !InConfig)
	{
		return;
	}
	StopDirector();
	Config = InConfig;
	RandomStream.Initialize(Seed);
	bRunning = true;
	CurrentWave = 0;
	CollectSpawners();
	PreloadTiersFrom(1);
	BeginBalanceLog();

	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 波次开始，种子 %d，可用刷怪器 %d 个"), Seed, Spawners.Num());
	if (Spawners.IsEmpty())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] 场景中没有勾选 bManagedByWaveDirector 的 ACC_EnemySpawner，不会刷怪。"));
	}

	ScheduleNextWave(Config->Waves.FirstWaveDelay);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CleanupTimer, this, &ThisClass::CleanupStrayEnemies, CleanupInterval, true);
	}
}

void UCC_WaveDirectorComponent::StopDirector()
{
	if (bRunning && CurrentWave > 0)
	{
		WriteBalanceRow();
	}
	bRunning = false;
	SpawnQueue.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimer);
		World->GetTimerManager().ClearTimer(QueueTimer);
		World->GetTimerManager().ClearTimer(CleanupTimer);
	}
	if (GetOwner() && GetOwner()->HasAuthority() && NextWaveServerTime != 0.0)
	{
		NextWaveServerTime = 0.0;
		NotifyChanged();
	}
}

// ---------------------------------------------------------------------------
// 波次
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::ScheduleNextWave(float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !bRunning)
	{
		return;
	}
	const float SafeDelay = FMath::Max(0.1f, Delay);
	World->GetTimerManager().SetTimer(WaveTimer, this, &ThisClass::AdvanceWave, SafeDelay, false);
	NextWaveServerTime = GetServerTime() + SafeDelay;
	NotifyChanged();
}

void UCC_WaveDirectorComponent::AdvanceWave()
{
	if (!bRunning || !Config)
	{
		return;
	}
	if (CurrentWave > 0)
	{
		WriteBalanceRow();
	}
	++CurrentWave;
	PruneDeadEnemies();

	const FCC_WaveTier* Tier = UCC_SurvivalModeConfig::FindTier(Config->Waves, CurrentWave);
	const int32 Players = FMath::Max(1, CountAlivePlayers());
	const float CountScale = UCC_SurvivalModeConfig::EvaluateMultiplier(Config->PlayerScaling.CountMultiplier, Players)
		* CCSurvivalCVars::GetSpawnCountScale();
	const float HealthScale = UCC_SurvivalModeConfig::EvaluateMultiplier(Config->PlayerScaling.HealthMultiplier, Players);

	int32 Requested = Tier ? UCC_SurvivalModeConfig::ComputeCount(*Tier, CurrentWave, CountScale) : 0;
	const int32 Room = FMath::Max(0, Config->Waves.MaxAliveEnemies - AliveEnemies.Num() - SpawnQueue.Num());
	const int32 Count = FMath::Min(Requested, Room);

	// 本波统计起点
	WaveStartGameTime = GetWorld()->GetTimeSeconds();
	SpawnedThisWave = 0;
	PlayersThisWave = Players;
	ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>();
	KillsAtWaveStart = GameState ? GameState->GetTeamStat(CCSurvivalTags::StatKills) : 0;
	DeathsAtWaveStart = GameState ? GameState->GetTeamStat(CCSurvivalTags::StatDeaths) : 0;

	FCC_EnemySpawnParams Params;
	if (Tier)
	{
		Params.InitEffect = Tier->InitEffect;
		Params.ExperienceMultiplier = Tier->ExperienceMultiplier;
	}
	Params.EffectLevel = static_cast<float>(CurrentWave);
	Params.HealthScale = HealthScale;
	Params.WaveIndex = CurrentWave;
	Params.bIsSet = true;

	if (Spawners.IsEmpty())
	{
		CollectSpawners();
	}
	TArray<FVector> PlayerLocations;
	GatherAlivePlayerLocations(PlayerLocations);
	const FCC_SpawnPacingSettings& Pacing = Config->Spawning;
	const double ReadyTime = GetWorld()->GetTimeSeconds() + Pacing.TelegraphDuration;
	int32 Queued = 0;
	for (int32 Index = 0; Tier && Index < Count; ++Index)
	{
		const TSubclassOf<ACC_EnemyCharacter> EnemyClass = PickEnemyClass(*Tier);
		ACC_EnemySpawner* Spawner = EnemyClass ? PickSpawner(PlayerLocations) : nullptr;
		FVector Location;
		if (!Spawner || !Spawner->FindSpawnLocation(EnemyClass, Pacing.bProjectToNavigation, RandomStream, Location))
		{
			continue;
		}
		FCC_PendingSpawn& Entry = SpawnQueue.AddDefaulted_GetRef();
		Entry.Spawner = Spawner;
		Entry.EnemyClass = EnemyClass;
		Entry.Location = Location;
		Entry.ReadyTime = ReadyTime;
		Entry.Params = Params;
		++Queued;
		if (Pacing.TelegraphDuration > 0.f)
		{
			MulticastSpawnTelegraph(Location, Pacing.TelegraphDuration, Pacing.TelegraphCue);
		}
	}
	EnsureQueueTimer();

	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 第 %d 波：档位 StartWave=%d，人数 %d，CountScale %.2f，计划 %d（上限截断后 %d），入队 %d，HealthScale %.2f"),
		CurrentWave, Tier ? Tier->StartWave : 0, Players, CountScale, Requested, Count, Queued, HealthScale);
	if (Requested > Room)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 场上已达上限 %d，本波少刷 %d 只"), Config->Waves.MaxAliveEnemies, Requested - Room);
	}

	if (GameState)
	{
		GameState->BroadcastMatchMessage(CCSurvivalTags::MsgWaveStarted, nullptr, nullptr, static_cast<float>(CurrentWave));
	}
	PreloadTiersFrom(CurrentWave + 1);
	ScheduleNextWave(Config->Waves.WaveInterval * CCSurvivalCVars::GetWaveIntervalScale());
}

// ---------------------------------------------------------------------------
// 分帧生成
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::EnsureQueueTimer()
{
	UWorld* World = GetWorld();
	if (!World || SpawnQueue.IsEmpty() || World->GetTimerManager().IsTimerActive(QueueTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(QueueTimer, this, &ThisClass::TickSpawnQueue,
		FMath::Max(0.02f, Config ? Config->Spawning.TickInterval : 0.1f), true);
}

void UCC_WaveDirectorComponent::TickSpawnQueue()
{
	UWorld* World = GetWorld();
	if (!World || !bRunning || !Config)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const int32 Budget = FMath::Max(1, Config->Spawning.SpawnsPerTick);
	int32 Spawned = 0;
	ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>();

	for (int32 Index = 0; Index < SpawnQueue.Num() && Spawned < Budget;)
	{
		if (SpawnQueue[Index].ReadyTime > Now)
		{
			++Index;
			continue;
		}
		const FCC_PendingSpawn Entry = SpawnQueue[Index];
		SpawnQueue.RemoveAt(Index);
		ACC_EnemySpawner* Spawner = Entry.Spawner.Get();
		ACC_EnemyCharacter* Enemy = Spawner ? Spawner->SpawnEnemyAt(Entry.EnemyClass, Entry.Location, Entry.Params) : nullptr;
		if (!Enemy)
		{
			UE_LOG(LogGAS_Demo, Verbose, TEXT("[Survival] 落点 %s 生成失败（被占用或刷怪器已移除）"), *Entry.Location.ToCompactString());
			continue;
		}
		++Spawned;
		++SpawnedThisWave;
		if (GameState)
		{
			GameState->AddTeamStat(CCSurvivalTags::StatEnemiesSpawned, 1);
		}
		// BeginPlay 期间就死掉的极端情况不计入存活。
		if (Enemy->IsAlive())
		{
			AliveEnemies.Add(Enemy);
		}
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[Survival] 生成 %s Wave=%d Level=%.0f HealthScale=%.2f XP×%.2f"),
			*GetNameSafe(Enemy), Entry.Params.WaveIndex, Entry.Params.EffectLevel, Entry.Params.HealthScale,
			Entry.Params.ExperienceMultiplier);
	}
	if (Spawned > 0)
	{
		UpdateAliveCount();
	}
	if (SpawnQueue.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(QueueTimer);
	}
}

// ---------------------------------------------------------------------------
// 存活追踪与回收
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::NotifyEnemyRemoved(ACC_EnemyCharacter* Enemy)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	const bool bRemoved = AliveEnemies.Remove(Enemy) > 0;
	FarTimes.Remove(Enemy);
	if (!bRemoved)
	{
		return;
	}
	UpdateAliveCount();
	if (bRunning && Config && Config->Waves.bAdvanceWhenCleared && CurrentWave > 0
		&& AliveEnemies.IsEmpty() && SpawnQueue.IsEmpty())
	{
		// 当前仍在死亡事件栈中，下一帧再推进，避免在伤害结算中途生成新怪。
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::AdvanceWave);
	}
}

TArray<ACC_EnemyCharacter*> UCC_WaveDirectorComponent::GetAliveEnemies() const
{
	TArray<ACC_EnemyCharacter*> Result;
	for (const TWeakObjectPtr<ACC_EnemyCharacter>& Weak : AliveEnemies)
	{
		if (ACC_EnemyCharacter* Enemy = Weak.Get())
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

void UCC_WaveDirectorComponent::PruneDeadEnemies()
{
	for (auto It = AliveEnemies.CreateIterator(); It; ++It)
	{
		const ACC_EnemyCharacter* Enemy = It->Get();
		if (!Enemy || !Enemy->IsAlive() || Enemy->IsActorBeingDestroyed())
		{
			FarTimes.Remove(*It);
			It.RemoveCurrent();
		}
	}
}

void UCC_WaveDirectorComponent::CleanupStrayEnemies()
{
	if (!bRunning || !Config)
	{
		return;
	}
	PruneDeadEnemies();
	const FCC_SpawnPacingSettings& Pacing = Config->Spawning;
	TArray<FVector> PlayerLocations;
	GatherAlivePlayerLocations(PlayerLocations);
	if (Pacing.LeashDistance > 0.f && !PlayerLocations.IsEmpty())
	{
		const float LeashSq = FMath::Square(Pacing.LeashDistance);
		TArray<ACC_EnemyCharacter*> ToRecycle;
		for (const TWeakObjectPtr<ACC_EnemyCharacter>& Weak : AliveEnemies)
		{
			ACC_EnemyCharacter* Enemy = Weak.Get();
			if (!Enemy)
			{
				continue;
			}
			float MinDistSq = TNumericLimits<float>::Max();
			for (const FVector& PlayerLocation : PlayerLocations)
			{
				MinDistSq = FMath::Min(MinDistSq, static_cast<float>(FVector::DistSquared(PlayerLocation, Enemy->GetActorLocation())));
			}
			if (MinDistSq <= LeashSq)
			{
				FarTimes.Remove(Weak);
				continue;
			}
			float& FarTime = FarTimes.FindOrAdd(Weak);
			FarTime += CleanupInterval;
			if (FarTime >= Pacing.LeashTime)
			{
				ToRecycle.Add(Enemy);
			}
		}
		for (ACC_EnemyCharacter* Enemy : ToRecycle)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 回收远离玩家的怪物 %s（不给经验）"), *GetNameSafe(Enemy));
			AliveEnemies.Remove(Enemy);
			FarTimes.Remove(Enemy);
			Enemy->Destroy();
		}
	}
	UpdateAliveCount();
}

void UCC_WaveDirectorComponent::UpdateAliveCount()
{
	const int32 NewCount = AliveEnemies.Num();
	if (AliveEnemyCount != NewCount)
	{
		AliveEnemyCount = NewCount;
		NotifyChanged();
	}
}

// ---------------------------------------------------------------------------
// 选择刷怪器与怪物
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::CollectSpawners()
{
	Spawners.Reset();
	for (TActorIterator<ACC_EnemySpawner> It(GetWorld()); It; ++It)
	{
		if (It->bManagedByWaveDirector)
		{
			Spawners.Add(*It);
		}
	}
}

ACC_EnemySpawner* UCC_WaveDirectorComponent::PickSpawner(const TArray<FVector>& PlayerLocations)
{
	const FCC_SpawnPacingSettings& Pacing = Config->Spawning;
	TArray<ACC_EnemySpawner*> Preferred;
	TArray<ACC_EnemySpawner*> Allowed;
	ACC_EnemySpawner* Farthest = nullptr;
	float FarthestDist = -1.f;

	for (const TWeakObjectPtr<ACC_EnemySpawner>& Weak : Spawners)
	{
		ACC_EnemySpawner* Spawner = Weak.Get();
		if (!Spawner)
		{
			continue;
		}
		float MinDist = TNumericLimits<float>::Max();
		for (const FVector& PlayerLocation : PlayerLocations)
		{
			MinDist = FMath::Min(MinDist, static_cast<float>(FVector::Dist2D(PlayerLocation, Spawner->GetActorLocation())));
		}
		if (MinDist > FarthestDist)
		{
			FarthestDist = MinDist;
			Farthest = Spawner;
		}
		if (MinDist >= Pacing.MinDistanceToPlayers)
		{
			Allowed.Add(Spawner);
			if (Pacing.PreferredDistanceToPlayers > 0.f && MinDist >= Pacing.PreferredDistanceToPlayers)
			{
				Preferred.Add(Spawner);
			}
		}
	}
	// 逐级放宽：离屏优先 → 满足最小距离 → 最远的那个，不跳过本波。
	const TArray<ACC_EnemySpawner*>& Pool = !Preferred.IsEmpty() ? Preferred : Allowed;
	if (!Pool.IsEmpty())
	{
		return Pool[RandomStream.RandRange(0, Pool.Num() - 1)];
	}
	return Farthest;
}

TSubclassOf<ACC_EnemyCharacter> UCC_WaveDirectorComponent::PickEnemyClass(const FCC_WaveTier& Tier)
{
	float TotalWeight = 0.f;
	for (const FCC_WeightedEnemy& Entry : Tier.Enemies)
	{
		TotalWeight += Entry.EnemyClass.IsNull() ? 0.f : FMath::Max(0.f, Entry.Weight);
	}
	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}
	float Roll = RandomStream.FRandRange(0.f, TotalWeight);
	const FCC_WeightedEnemy* Chosen = nullptr;
	for (const FCC_WeightedEnemy& Entry : Tier.Enemies)
	{
		if (Entry.EnemyClass.IsNull() || Entry.Weight <= 0.f)
		{
			continue;
		}
		Chosen = &Entry;
		Roll -= Entry.Weight;
		if (Roll <= 0.f)
		{
			break;
		}
	}
	if (!Chosen)
	{
		return nullptr;
	}
	UClass* Loaded = Chosen->EnemyClass.Get();
	if (!Loaded)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] %s 未预加载完成，同步加载（可能卡顿）"), *Chosen->EnemyClass.ToString());
		Loaded = Chosen->EnemyClass.LoadSynchronous();
	}
	if (Loaded)
	{
		LoadedEnemyClasses.AddUnique(Loaded);
	}
	return Loaded;
}

void UCC_WaveDirectorComponent::PreloadTiersFrom(int32 Wave)
{
	if (!Config)
	{
		return;
	}
	// 预加载 Wave 所在档与下一档。
	TArray<FSoftObjectPath> Paths;
	const FCC_WaveTier* Current = UCC_SurvivalModeConfig::FindTier(Config->Waves, Wave);
	for (int32 Index = 0; Current && Index < Config->Waves.Tiers.Num(); ++Index)
	{
		if (&Config->Waves.Tiers[Index] != Current)
		{
			continue;
		}
		for (int32 Offset = 0; Offset <= 1 && Config->Waves.Tiers.IsValidIndex(Index + Offset); ++Offset)
		{
			for (const FCC_WeightedEnemy& Entry : Config->Waves.Tiers[Index + Offset].Enemies)
			{
				if (!Entry.EnemyClass.IsNull() && !Entry.EnemyClass.Get())
				{
					Paths.AddUnique(Entry.EnemyClass.ToSoftObjectPath());
				}
			}
		}
		break;
	}
	if (!Paths.IsEmpty())
	{
		PreloadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths));
	}
}

// ---------------------------------------------------------------------------
// 玩家查询
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::GatherAlivePlayerLocations(TArray<FVector>& OutLocations) const
{
	if (const ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>())
	{
		for (const ACC_SurvivalPlayerState* PS : GameState->GetSurvivalPlayers())
		{
			const APawn* Pawn = PS->GetPawn();
			if (Pawn && PS->IsAliveInMatch())
			{
				OutLocations.Add(Pawn->GetActorLocation());
			}
		}
	}
}

int32 UCC_WaveDirectorComponent::CountAlivePlayers() const
{
	int32 Count = 0;
	if (const ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>())
	{
		for (const ACC_SurvivalPlayerState* PS : GameState->GetSurvivalPlayers())
		{
			Count += PS->IsAliveInMatch() ? 1 : 0;
		}
	}
	return Count;
}

double UCC_WaveDirectorComponent::GetServerTime() const
{
	const ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>();
	return GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
}

// ---------------------------------------------------------------------------
// 复制与表现
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::NotifyChanged()
{
	GetOwner()->ForceNetUpdate();
	OnWaveStateChanged.Broadcast();
}

void UCC_WaveDirectorComponent::OnRep_WaveState()
{
	OnWaveStateChanged.Broadcast();
}

void UCC_WaveDirectorComponent::MulticastSpawnTelegraph_Implementation(const FVector_NetQuantize& Location, float Duration, const FGameplayTag& CueTag)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	OnSpawnTelegraph.Broadcast(Location, Duration);
	if (CueTag.IsValid())
	{
		FGameplayCueParameters Parameters;
		Parameters.Location = Location;
		Parameters.RawMagnitude = Duration;
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(
			GetOwner(), CueTag, EGameplayCueEvent::Executed, Parameters);
	}
}

// ---------------------------------------------------------------------------
// 调试
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::DebugSetWave(int32 Wave)
{
	if (!bRunning)
	{
		return;
	}
	CurrentWave = FMath::Max(0, Wave - 1);
	AdvanceWave();
}

void UCC_WaveDirectorComponent::DebugSkipWave()
{
	if (bRunning)
	{
		AdvanceWave();
	}
}

void UCC_WaveDirectorComponent::DebugClearEnemies()
{
	SpawnQueue.Reset();
	for (ACC_EnemyCharacter* Enemy : GetAliveEnemies())
	{
		Enemy->Destroy();
	}
	AliveEnemies.Reset();
	FarTimes.Reset();
	UpdateAliveCount();
}

// ---------------------------------------------------------------------------
// 平衡数据 CSV（开发构建）
// ---------------------------------------------------------------------------

void UCC_WaveDirectorComponent::BeginBalanceLog()
{
#if !UE_BUILD_SHIPPING
	BalanceCsvPath = FPaths::ProjectSavedDir() / TEXT("Survival")
		/ FString::Printf(TEXT("Survival_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FFileHelper::SaveStringToFile(TEXT("Wave,Players,Spawned,Kills,Deaths,DurationSeconds,AliveAtEnd\n"), *BalanceCsvPath);
#endif
}

void UCC_WaveDirectorComponent::WriteBalanceRow()
{
#if !UE_BUILD_SHIPPING
	if (BalanceCsvPath.IsEmpty() || !GetWorld())
	{
		return;
	}
	const ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>();
	const int32 Kills = GameState ? GameState->GetTeamStat(CCSurvivalTags::StatKills) - KillsAtWaveStart : 0;
	const int32 Deaths = GameState ? GameState->GetTeamStat(CCSurvivalTags::StatDeaths) - DeathsAtWaveStart : 0;
	const FString Row = FString::Printf(TEXT("%d,%d,%d,%d,%d,%.1f,%d\n"), CurrentWave, PlayersThisWave, SpawnedThisWave,
		Kills, Deaths, GetWorld()->GetTimeSeconds() - WaveStartGameTime, AliveEnemies.Num());
	FFileHelper::SaveStringToFile(Row, *BalanceCsvPath, FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(), FILEWRITE_Append);
#endif
}
