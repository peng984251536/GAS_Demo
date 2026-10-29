#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "Data/CC_EnemySpawnTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/NetSerialization.h"
#include "Engine/TimerHandle.h"
#include "CC_WaveDirectorComponent.generated.h"

class ACC_EnemyCharacter;
class ACC_EnemySpawner;
class UCC_SurvivalModeConfig;
struct FCC_WaveTier;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_WaveStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_SpawnTelegraph, FVector, Location, float, Duration);

/** 已确定落点、等待预警结束后生成的一只怪物（仅服务器）。 */
USTRUCT()
struct FCC_PendingSpawn
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ACC_EnemySpawner> Spawner;

	UPROPERTY()
	TSubclassOf<ACC_EnemyCharacter> EnemyClass;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** 世界游戏时间（暂停时不走）。 */
	double ReadyTime = 0.0;

	UPROPERTY()
	FCC_EnemySpawnParams Params;
};

/**
 * 波次调度（挂在 ACC_SurvivalGameState 上）。
 * 服务器：定时推进波次 → 按档位与人数算数量 → 选刷怪器与落点 → 放入队列并播出生预警 → 分帧生成；
 *         追踪存活怪物，回收长时间远离所有玩家的怪物；开发构建下每波写一行平衡数据 CSV。
 * 复制：当前波次、下一波的服务器时间、存活怪物数，供 HUD 显示。
 * 只使用勾选了 bManagedByWaveDirector 的 ACC_EnemySpawner。
 */
UCLASS(ClassGroup = (Survival))
class GAS_DEMO_API UCC_WaveDirectorComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UCC_WaveDirectorComponent(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Survival|Wave")
	int32 GetCurrentWave() const { return CurrentWave; }

	/** 下一波开始的服务器时间；配合 ACC_SurvivalGameState::GetSecondsUntilServerTime 显示倒计时。 */
	UFUNCTION(BlueprintPure, Category = "Survival|Wave")
	double GetNextWaveServerTime() const { return NextWaveServerTime; }

	UFUNCTION(BlueprintPure, Category = "Survival|Wave")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

	UPROPERTY(BlueprintAssignable, Category = "Survival|Wave")
	FCC_WaveStateChanged OnWaveStateChanged;

	/** 客户端收到出生预警（另有 GameplayCue 时两者都会触发），蓝图可自己画地面提示。 */
	UPROPERTY(BlueprintAssignable, Category = "Survival|Wave")
	FCC_SpawnTelegraph OnSpawnTelegraph;

	// --- 仅服务器 ---

	void StartDirector(const UCC_SurvivalModeConfig* InConfig, int32 Seed);
	/** 停止计时、清空队列；已在场的怪物保留（由 GameMode 决定是否冻结）。 */
	void StopDirector();
	/** 怪物死亡（DeathStarted）时由 GameMode 调用。 */
	void NotifyEnemyRemoved(ACC_EnemyCharacter* Enemy);
	/** 当前存活（已生成且未死亡）的怪物。 */
	TArray<ACC_EnemyCharacter*> GetAliveEnemies() const;
	int32 GetQueuedSpawnCount() const { return SpawnQueue.Num(); }
	bool IsRunning() const { return bRunning; }

	// --- 调试（仅服务器、开发构建使用） ---

	void DebugSetWave(int32 Wave);
	void DebugSkipWave();
	/** 直接销毁所有怪物并清空队列（不给经验、不计击杀）。 */
	void DebugClearEnemies();

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnTelegraph(const FVector_NetQuantize& Location, float Duration, const FGameplayTag& CueTag);

private:
	UFUNCTION()
	void OnRep_WaveState();
	void NotifyChanged();

	void AdvanceWave();
	void ScheduleNextWave(float Delay);
	void TickSpawnQueue();
	void EnsureQueueTimer();
	void CleanupStrayEnemies();
	void PruneDeadEnemies();
	void UpdateAliveCount();

	void CollectSpawners();
	ACC_EnemySpawner* PickSpawner(const TArray<FVector>& PlayerLocations);
	TSubclassOf<ACC_EnemyCharacter> PickEnemyClass(const FCC_WaveTier& Tier);
	void PreloadTiersFrom(int32 Wave);
	void GatherAlivePlayerLocations(TArray<FVector>& OutLocations) const;
	int32 CountAlivePlayers() const;
	double GetServerTime() const;

	void BeginBalanceLog();
	void WriteBalanceRow();

	UPROPERTY(ReplicatedUsing = OnRep_WaveState)
	int32 CurrentWave = 0;

	UPROPERTY(ReplicatedUsing = OnRep_WaveState)
	double NextWaveServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_WaveState)
	int32 AliveEnemyCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<const UCC_SurvivalModeConfig> Config;

	/** 用到的怪物类保持加载（软引用同步加载的兜底结果）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UClass>> LoadedEnemyClasses;

	UPROPERTY(Transient)
	TArray<FCC_PendingSpawn> SpawnQueue;

	TArray<TWeakObjectPtr<ACC_EnemySpawner>> Spawners;
	TSet<TWeakObjectPtr<ACC_EnemyCharacter>> AliveEnemies;
	TMap<TWeakObjectPtr<ACC_EnemyCharacter>, float> FarTimes;
	TArray<TSharedPtr<FStreamableHandle>> PreloadHandles;
	FRandomStream RandomStream;
	bool bRunning = false;

	FTimerHandle WaveTimer;
	FTimerHandle QueueTimer;
	FTimerHandle CleanupTimer;

	// 平衡数据（开发构建）
	FString BalanceCsvPath;
	double WaveStartGameTime = 0.0;
	int32 KillsAtWaveStart = 0;
	int32 DeathsAtWaveStart = 0;
	int32 SpawnedThisWave = 0;
	int32 PlayersThisWave = 0;
};
