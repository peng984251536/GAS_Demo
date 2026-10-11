#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "NativeGameplayTags.h"
#include "CC_DamageTypes.generated.h"

/** 技能参数与免疫标签；名称沿用现有约定，调用者无需包含伤害 Execution 头文件。 */
namespace StandaloneDamageTags
{
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlatDamage);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillScale);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistanceMultiplier);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitMultiplier);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Healing);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageImmunity);
}


/** 暴击策略：Disabled 禁用；Random 由服务器按暴击率随机；Forced 表示服务器已确认的必定暴击。 */
UENUM(BlueprintType)
enum class ECC_CriticalPolicy : uint8 { Disabled, Random, Forced };
UENUM(BlueprintType)
/** 应用状态：输入无效、非权威调用、GE 拒绝或 GE 已接受；Applied 不保证产生正伤害。 */
enum class ECC_DamageApplyStatus : uint8 { InvalidInput, NotAuthority, Rejected, Applied };

/**
 * 技能提交的一次命中请求。由服务器认可参数后交给 ApplyDamage，不填最终伤害。
 * 技能系数与固定伤害描述技能；增减伤、防御和暴击属性由 Execution 从 GAS 读取。
 * 伤害参数使用正数，治疗使用独立 ApplyHealing；客户端 RPC 中的 Forced 不能直接信任。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_DamageRequest
{
    GENERATED_BODY()
    /** 攻击力系数，1=100%、2=200%；纯固定伤害设为 0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SkillScale = 1.f;
    /** 额外固定伤害，仍受增伤、减伤和防御影响；不是无视防御的真实伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FlatDamage = 0.f;
    /** 已确定的距离衰减倍率，默认 1；系统不会自动测距或读取武器曲线。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DistanceMultiplier = 1.f;
    /** 已确认的命中部位倍率，默认 1；大于 1 不代表暴击。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeakPointMultiplier = 1.f;
    /** 本次攻击是否允许随机暴击、禁止暴击或服务器强制暴击。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECC_CriticalPolicy CriticalPolicy = ECC_CriticalPolicy::Random;
    /** 伤害分类标签，用于结果传递和飘字分组；目前没有自动元素抗性或配色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag DamageType;
    /** true 时从有效 HitResult 取飘字起点；false 使用目标包围盒顶部附近。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseHitLocation = false;
    /** HitResult 是否已由命中检测填好；true 才会写入 EffectContext。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasHitResult = false;
    /** 本次命中结果；只有 bHasHitResult=true 才使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FHitResult HitResult;
};

/**
 * 属性集完成一次伤害或治疗后的结果副本。
 * ResolvedDamage 表达完成攻防公式的数值，HealthDamage 表达本次实际生命变化。
 * 通过服务器本地 C++ 委托通知表现/日志；结构体本身不是 RPC，不自动复制。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_DamageResolved
{
    GENERATED_BODY()
    /** 攻击/治疗来源的 Avatar，通常是攻击者角色。 */
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Source = nullptr;
    /** 本次被修改生命值的目标 Avatar。 */
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) FGameplayTag DamageType;
    /** 结算时确定的世界空间飘字起点，不随目标后续移动。 */
    UPROPERTY(BlueprintReadOnly) FVector TextLocation = FVector::ZeroVector;
    /** 伤害时为增减伤后的完整伤害；治疗时为请求治疗量，尚未限制到缺失生命。 */
    UPROPERTY(BlueprintReadOnly) float ResolvedDamage = 0.f;
    /** 护盾吸收量的预留字段；当前未实现护盾，值为 0。 */
    UPROPERTY(BlueprintReadOnly) float ShieldAbsorbed = 0.f;
    /** 实际生命变化的绝对值：伤害为实际扣血，治疗为实际恢复量。 */
    UPROPERTY(BlueprintReadOnly) float HealthDamage = 0.f;
    /** 本次资源结算前的生命值。 */
    UPROPERTY(BlueprintReadOnly) float HealthBefore = 0.f;
    /** 本次资源结算完成时的生命值；后续回调可能再次改变生命。 */
    UPROPERTY(BlueprintReadOnly) float HealthAfter = 0.f;
    /** 服务器本次判定是否暴击；治疗固定为 false。 */
    UPROPERTY(BlueprintReadOnly) bool bCritical = false;
    /** 区分伤害与治疗，不能仅根据数值正负猜测。 */
    UPROPERTY(BlueprintReadOnly) bool bHealing = false;
    /** 本次伤害将生命从大于 0 降至 0；仅提供结果，不重复激活死亡能力。 */
    UPROPERTY(BlueprintReadOnly) bool bKilled = false;
};
/** 一次结算的服务器本地多播通知；使用 AddUObject/AddLambda 订阅，结束时解除。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCC_DamageResolvedDelegate, const FCC_DamageResolved&);
