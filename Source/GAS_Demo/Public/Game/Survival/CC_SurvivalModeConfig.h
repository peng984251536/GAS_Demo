#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "CC_SurvivalModeConfig.generated.h"

class ACC_EnemyCharacter;
class UCC_BuffOfferPool;
class UGameplayEffect;

/** 波次档位中的一种怪物及其抽取权重。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_WeightedEnemy
{
	GENERATED_BODY()

	/** 软引用：开局只预加载当前档和下一档。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSoftClassPtr<ACC_EnemyCharacter> EnemyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.01"))
	float Weight = 1.f;
};

/**
 * 一个波次档位：从 StartWave 起生效，直到下一档的 StartWave。
 * 档位之间做"质变"（换初始化 GE、换怪物组合）；档内每波通过 GE Level = 波次平滑变强。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_WaveTier
{
	GENERATED_BODY()

	/** 从第几波开始使用本档。第一档必须为 1，后续严格递增。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 StartWave = 1;

	/** 本档怪物的初始化属性 GE（施加等级 = 波次）；为空时用怪物自己的 CharacterConfig。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<UGameplayEffect> InitEffect;

	/** 本档怪物池，按权重抽取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (TitleProperty = "EnemyClass"))
	TArray<FCC_WeightedEnemy> Enemies;

	/** 本档第一波的数量（单人）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 BaseCount = 5;

	/** 本档内每多一波增加的数量，可为小数（累计后取整）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	float CountPerWave = 1.f;

	/** 本档怪物的经验倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	float ExperienceMultiplier = 1.f;
};

/** 波次节奏。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_WaveSettings
{
	GENERATED_BODY()

	/** 开局到第一波的秒数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0", Units = "s"))
	float FirstWaveDelay = 5.f;

	/** 两波之间的秒数（定时推进，不等清场）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1", Units = "s"))
	float WaveInterval = 20.f;

	/** 场上（含待生成队列）怪物上限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 MaxAliveEnemies = 60;

	/** 场上清空后立即进入下一波。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	bool bAdvanceWhenCleared = false;

	/** 按 StartWave 升序排列。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (TitleProperty = "StartWave"))
	TArray<FCC_WaveTier> Tiers;
};

/** 刷怪执行：分帧、预警、落点筛选、回收。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_SpawnPacingSettings
{
	GENERATED_BODY()

	/** 每次处理队列最多生成几只。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1"))
	int32 SpawnsPerTick = 2;

	/** 处理队列的间隔（游戏时间，暂停时停止）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.02", Units = "s"))
	float TickInterval = 0.1f;

	/** 出生预警时长：落点确定后多久真正生成。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0", Units = "s"))
	float TelegraphDuration = 0.8f;

	/** 预警时在落点执行的 GameplayCue（客户端本地执行，Location = 落点，RawMagnitude = 时长）。可空。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (Categories = "GameplayCue"))
	FGameplayTag TelegraphCue;

	/** 刷怪器离最近存活玩家至少多远才可用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0", Units = "cm"))
	float MinDistanceToPlayers = 800.f;

	/** 优先选择离所有玩家都超过此距离的刷怪器（俯视角下近似"屏幕外"）。0 表示不区分。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0", Units = "cm"))
	float PreferredDistanceToPlayers = 1600.f;

	/** 落点必须能投影到导航网格。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning")
	bool bProjectToNavigation = true;

	/** 离所有存活玩家都超过此距离的怪物开始计时回收。0 表示不回收。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0", Units = "cm"))
	float LeashDistance = 4000.f;

	/** 持续超出 LeashDistance 多久后回收（不给经验、不计击杀）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0", Units = "s"))
	float LeashTime = 8.f;
};

/** 人数缩放：X = 存活玩家数，Y = 倍率。曲线没有关键帧时视为 1。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_PlayerScalingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	FRuntimeFloatCurve CountMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	FRuntimeFloatCurve HealthMultiplier;
};

/** 共享经验与升级。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_ProgressionSettings
{
	GENERATED_BODY()

	/** X = 当前等级，Y = 升到下一级所需经验。没有关键帧时使用下面的线性公式。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	FRuntimeFloatCurve ExperienceToNext;

	/** 线性公式：1 级升 2 级所需经验。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "1"))
	float BaseExperienceToNext = 20.f;

	/** 线性公式：每升一级所需经验的增量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0"))
	float ExperienceToNextPerLevel = 10.f;

	/** 升级时每个玩家抽三选一的池。其中的 GE 必须 bRemoveOnDeath = false，否则阵亡即丢失。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UCC_BuffOfferPool> BuffPool;

	/** 等级上限，0 表示不限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0"))
	int32 MaxLevel = 0;

	/** 联机时选卡界面打开期间施加的 GE（如 IncomingDamageMultiplier ×0.5 或 Status.DamageImmunity）。可空。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TSubclassOf<UGameplayEffect> ChoosingProtectionEffect;
};

/** 阵亡复活。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_RespawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "s"))
	float BaseDelay = 5.f;

	/** 每多阵亡一次增加的等待。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "s"))
	float DelayPerDeath = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "s"))
	float MaxDelay = 20.f;

	/** 复活点离队友的最小/最大距离。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "cm"))
	float MinRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "cm"))
	float MaxRadius = 400.f;

	/** 倒计时结束时死亡动作仍未结束，最多再等多久（玩家没有死亡能力时的兜底）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", Units = "s"))
	float DeathFinishedTimeout = 5.f;

	/** 复活时施加：回满血 + 短暂 Status.DamageImmunity + 复活 Cue。为空时代码兜底回满血。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn")
	TSubclassOf<UGameplayEffect> ReviveEffect;
};

/**
 * 生存模式全部配置（只读，硬性规则 6）。由 ACC_SurvivalGameMode 的类默认值引用；
 * 地图 URL 选项 ?SurvivalConfig=/Game/.../DA_X.DA_X 可覆盖。
 * 纯计算函数放在这里，便于自动化测试。
 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_SurvivalModeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival")
	FCC_WaveSettings Waves;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival")
	FCC_SpawnPacingSettings Spawning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival")
	FCC_PlayerScalingSettings PlayerScaling;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival")
	FCC_ProgressionSettings Progression;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival")
	FCC_RespawnSettings Respawn;

	/** 第 Wave 波使用的档位：StartWave <= Wave 的最后一档；没有档位时为空。 */
	static const FCC_WaveTier* FindTier(const FCC_WaveSettings& Settings, int32 Wave);

	/** 第 Wave 波的数量：(BaseCount + CountPerWave × 档内波数) × CountScale，四舍五入，不小于 0。 */
	static int32 ComputeCount(const FCC_WaveTier& Tier, int32 Wave, float CountScale);

	/** 曲线没有关键帧时返回 1，否则返回 Curve(X)，不小于 0.01。 */
	static float EvaluateMultiplier(const FRuntimeFloatCurve& Curve, float X);

	/** 从 Level 升到 Level+1 所需经验，不小于 1。 */
	static float ComputeExperienceToNext(const FCC_ProgressionSettings& Settings, int32 Level);

	/**
	 * 加经验后的等级与剩余经验（纯函数，供组件与测试共用）。
	 * @return 本次升了几级。
	 */
	static int32 ApplyExperience(const FCC_ProgressionSettings& Settings, int32& InOutLevel, float& InOutExperience, float Amount);

	/** 第 DeathCount 次阵亡（从 1 开始）的复活等待。 */
	static float ComputeRespawnDelay(const FCC_RespawnSettings& Settings, int32 DeathCount);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
