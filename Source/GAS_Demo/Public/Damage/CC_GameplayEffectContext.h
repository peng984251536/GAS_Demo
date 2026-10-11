#pragma once
#include "GameplayEffectTypes.h"
#include "AbilitySystemGlobals.h"
#include "Damage/CC_DamageTypes.h"
#include "CC_GameplayEffectContext.generated.h"

/**
 * 一次 GE 的扩展上下文：携带伤害类型、暴击标记和飘字位置策略。
 * 基类负责来源、命中结果等通用数据；本类补充客户端表现需要的元数据。
 * 每次 Execution 复制 Context，避免多个目标或周期执行互相覆盖暴击结果。
 */
USTRUCT()
struct GAS_DEMO_API FCC_GameplayEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()
    /** 伤害分类，随 Context 复制，用于客户端分组。 */
    FGameplayTag DamageType;
    /** 仅服务器使用的暴击判定策略，不发送给客户端重新随机。 */
    ECC_CriticalPolicy CriticalPolicy = ECC_CriticalPolicy::Disabled;
    /** 服务器已经确定的暴击结果，客户端仅展示。 */
    bool bCritical = false;
    /** 是否优先从 HitResult 选择世界空间数字起点。 */
    bool bUseHitLocation = false;
    /**
     * 检查句柄的反射类型并安全取得项目上下文。
     * @param Handle 待检查的 GameplayEffectContext 句柄。
     * @return 只读项目上下文；空句柄或类型不符返回 nullptr。
     */
    static const FCC_GameplayEffectContext* Get(const FGameplayEffectContextHandle& Handle);
    /**
     * 复用类型检查，取得当次上下文的可写指针。
     * @param Handle 待检查的 GameplayEffectContext 句柄。
     * @return 可写项目上下文；空句柄或类型不符返回 nullptr。
     */
    static FCC_GameplayEffectContext* GetMutable(FGameplayEffectContextHandle& Handle);
    /**
     * 向 GAS 提供上下文的实际反射类型。
     * @return FCC_GameplayEffectContext 的反射结构体。
     */
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    /**
     * 复制上下文字段并深复制 HitResult，避免不同命中共享可变数据。
     * @return 新上下文指针，由调用方的句柄接管生命周期。
     */
    virtual FGameplayEffectContext* Duplicate() const override
    {
        auto* Copy = new FCC_GameplayEffectContext(*this);
        if (GetHitResult()) Copy->AddHitResult(*GetHitResult(), true);
        return Copy;
    }
    /**
     * 序列化基类数据、伤害类型及已判定的表现标志，不在客户端重新判定暴击。
     * @param Ar 读取或写入上下文的网络序列化 Archive。
     * @param Map 用于网络对象引用映射的 PackageMap。
     * @param bOutSuccess 输出序列化是否成功，不等同于对象引用已经全部完成映射。
     * @return 对象引用均完成映射时返回 true，否则返回 false。
     */
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};
// 启用自定义网络序列化与结构体复制；仅声明字段不能保证 Context 扩展字段正确复制。
template<> struct TStructOpsTypeTraits<FCC_GameplayEffectContext> : TStructOpsTypeTraitsBase2<FCC_GameplayEffectContext>
{
    enum { WithNetSerializer = true, WithCopy = true };
};
/**
 * 项目 GAS 全局工厂。让 ASC::MakeEffectContext 创建 FCC_GameplayEffectContext。
 * 类通过 DefaultGame.ini 配置；更换工厂需要完整重启编辑器。
 */
UCLASS()
class GAS_DEMO_API UCC_AbilitySystemGlobals : public UAbilitySystemGlobals
{
    GENERATED_BODY()
public:
    /**
     * 为 ASC 分配新的项目扩展上下文。
     * @return 新 FCC_GameplayEffectContext 指针，由 ContextHandle 管理。
     */
    virtual FGameplayEffectContext* AllocGameplayEffectContext() const override { return new FCC_GameplayEffectContext(); }
};
