#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "CC_DamageTypes.generated.h"

/** 暴击策略：Disabled 不暴击；Random 由服务器按暴击率随机；Forced 为服务器已确认的必定暴击。 */
UENUM(BlueprintType)
enum class ECC_CriticalPolicy : uint8 { Disabled, Random, Forced };

/** ApplyDamage / ApplyHealing 的结果。Applied 只表示 GE 已被接受，不保证产生了伤害。 */
UENUM(BlueprintType)
enum class ECC_DamageApplyStatus : uint8 { InvalidInput, NotAuthority, Rejected, Applied };

/**
 * 技能提交的一次伤害请求，只描述技能本身；攻防、增减伤、暴击属性由 Execution 从 GAS 读取。
 * 所有数值为非负数，治疗使用 ApplyHealing。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_DamageRequest
{
	GENERATED_BODY()
	/** 攻击力系数：1 = 100%，2 = 200%；纯固定伤害填 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float SkillScale = 1.f;
	/** 额外固定伤害，仍受增减伤和防御影响。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float FlatDamage = 0.f;
	/** 技能侧已算好的额外倍率，例如距离衰减 × 弱点倍率；不代表暴击。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float DamageMultiplier = 1.f;
	/** 是否允许暴击。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") ECC_CriticalPolicy CriticalPolicy = ECC_CriticalPolicy::Random;
	/**
	 * 伤害类型（Damage.Type.*）。写进 Spec 标签：GE 修正器可以据此做条件增减伤（如火抗），飘字据此选样式。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(Categories="Damage.Type")) FGameplayTag DamageType;
	/** 来源标签（Damage.Source.*），同样写进 Spec 标签，供 GE 条件修正和 Buff 使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") FGameplayTagContainer SourceTags;
	/** HitResult 是否有效；有效时写进 Context，飘字从命中点出现。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") bool bHasHitResult = false;
	/** 本次命中信息，bHasHitResult 为 true 时使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(EditCondition="bHasHitResult")) FHitResult HitResult;
};
