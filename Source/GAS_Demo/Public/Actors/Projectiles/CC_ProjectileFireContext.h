#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CC_ProjectileFireContext.generated.h"

class ACC_ArrowProjectile;

/** 发射参数按值写入每支箭；命中阶段不查询射手的 Buff。 */
USTRUCT(BlueprintType)

struct GAS_DEMO_API FCC_ArrowPayload
{
    GENERATED_BODY()
    /** 合并：相乘；攻击力系数由技能提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SkillScale = 1.f;
    /** 合并：相加；技能提供的固定伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FlatDamage = 10.f;
    /** 合并：相乘；同时缩放攻击力系数与固定伤害，避免只缩放部分伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageMultiplier = 1.f;
    /** 合并：标签集合并集；记录本次伤害的来源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer SourceTags;
};

/** 放箭决策点的数据；数学展开和实际生成均由射击系统负责。 */
USTRUCT(BlueprintType)

struct GAS_DEMO_API FCC_ProjectileFireContext
{
    GENERATED_BODY()
    /** 只读：射击系统提供出箭位置、方向和基础配置。 */
    UPROPERTY(BlueprintReadOnly) FVector Origin = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector AimDirection = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly) TSubclassOf<ACC_ArrowProjectile> ProjectileClass;
    UPROPERTY(BlueprintReadOnly) float Speed = 1800.f;
    /** 可修改；合并：相加；最终发射数量限制在 1～32。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ProjectileCount = 1;
    /** 合并：取最大；水平扇形的总夹角，最终限制在 0～180 度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadAngle = 0.f;
    /** 合并：相加；连续发射轮数，最终限制 1～8。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VolleyCount = 1;
    /** 合并：取最大；连续轮次间隔，单位秒。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VolleyInterval = 0.f;
    /** 合并：按 Payload 各字段规则分别合并。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCC_ArrowPayload Payload;

    /**
     * 校验本次参数并把数量、散射展开为对称的单位方向；失败时返回空数组。
     * @param OutDirections 每支箭的世界方向；奇数数量时保留正中的原始方向。
     */
    void Resolve(TArray<FVector>& OutDirections) const;
};
