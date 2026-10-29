#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "CC_EnemySpawnTypes.generated.h"

class UGameplayEffect;

/**
 * 生成怪物时由调用方（例如生存模式的波次系统）指定的实例参数。
 * 在 SpawnActorDeferred 与 FinishSpawning 之间写入怪物，BeginPlay 初始化属性时读取。
 * 与具体玩法模式无关：角色只知道"用哪个 GE、什么等级初始化，血量再乘多少"。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_EnemySpawnParams
{
	GENERATED_BODY()

	/** 初始化属性 GE；为空时使用 CharacterConfig.InitializeAttributesEffect。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<UGameplayEffect> InitEffect;

	/** 初始化 GE 的等级（生存模式 = 波次），GE 里的 ScalableFloat 按它读曲线。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	float EffectLevel = 1.f;

	/** 初始化后对 MaxHealth 与 Health 的额外倍率（人数缩放），1 表示不变。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.01"))
	float HealthScale = 1.f;

	/** 被击杀时经验奖励的倍率，乘在 CharacterConfig.ExperienceReward 上。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float ExperienceMultiplier = 1.f;

	/** 生成时所在的波次，0 表示不是由波次系统生成。只用于日志与统计。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 WaveIndex = 0;

	/** 是否由调用方显式设置过；未设置时角色完全沿用原配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bIsSet = false;
};
