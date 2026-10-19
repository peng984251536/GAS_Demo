#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CC_BuffTypes.generated.h"
class UCC_BuffComponent;
class UCC_BuffBehavior;
/**
 * 规则实例状态机；Disabled 可以恢复，Finished 是终态。
 */
UENUM(BlueprintType)
enum class ECC_BuffBehaviorState : uint8
{
    Uninitialized,
    Disabled,
    Enabled,
    Finished
};
/**
 * 规则停止执行的原因；禁用不一定意味着 GE 已被移除。
 */
UENUM(BlueprintType)
enum class ECC_BuffDisableReason : uint8
{
    Inhibited,
    OwnerDeath,
    Ending
};
/**
 * 最终结束原因；EffectRemoved 同时覆盖 GAS 到期和未指定原因的外部移除。
 */
UENUM(BlueprintType)
enum class ECC_BuffEndReason : uint8
{
    EffectRemoved,
    OwnerDeath,
    Reset,
    ASCReplaced,
    EndPlay
};
/**
 * WhileEnabled：禁用即回收；Instance：最终结束回收；Independent：不由父 Buff 管理。
 */
UENUM(BlueprintType)
enum class ECC_BuffResourceLifetime : uint8
{
    WhileEnabled,
    Instance,
    Independent
};

/**
 * 单个 Buff 的按值快照：在 GAS 回调中重建后交给规则读取。
 * 不持有 ActiveGameplayEffect 内部地址，避免堆叠、移除和同步重入让地址失效。
 * SourceASC 描述来源，TargetASC 描述持有者；两者可能是同一个 ASC。
 */
USTRUCT(BlueprintType)

struct GAS_DEMO_API FCC_BuffInstanceView
{
    GENERATED_BODY()
    /**
     * 当前持有 Buff 的角色；弱引用不阻止角色销毁。
     */
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Owner;
    /**
     * 原始施加者，不一定与持有 Buff 的角色相同。
     */
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
    /**
     * GAS 活动 GE 的标识；同一 GE 堆叠时通常保持不变。
     */
    UPROPERTY(BlueprintReadOnly) FActiveGameplayEffectHandle Handle;
    /**
     * 组件分配的展示编号，用于 UI 查询与移除；不能当作数组下标。
     */
    UPROPERTY(BlueprintReadOnly) int32 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly) int32 StackCount = 0;
    UPROPERTY(BlueprintReadOnly) float Level = 1.f;
    UPROPERTY(BlueprintReadOnly) TMap<FGameplayTag, float> SetByCaller;

    /**
     * 读取本次施加时携带的参数，不修改 GE 模板。
     * @param Tag 参数标签。
     * @param Default 未提供此参数时返回的默认值。
     * @return 快照中的参数值或默认值。
     */
    float GetSetByCaller(FGameplayTag Tag, float Default = 0.f) const
    {
        const float* Value = SetByCaller.Find(Tag);
        return Value ? *Value : Default;
    }
};

/**
 * 供 UI 使用的复制条目，与服务器 Behavior 无关；InstanceId 是组件内的稳定标识。
 */
USTRUCT(BlueprintType)

struct GAS_DEMO_API FCC_ReplicatedBuffItem : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly) TSubclassOf<UGameplayEffect> EffectClass;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BuffTags;
    UPROPERTY(BlueprintReadOnly) int32 StackCount = 1;
    UPROPERTY(BlueprintReadOnly) float Level = 1.f;
    UPROPERTY(BlueprintReadOnly) bool bIsInhibited = false;
    /**
     * 服务器时钟上的到期时刻；负数代表无限持续，用于本地推算倒计时。
     */
    UPROPERTY(BlueprintReadOnly) float EndServerTime = -1.f;
    /**
     * GE 总持续秒数；负数代表无限持续，不是当前剩余时间。
     */
    UPROPERTY(BlueprintReadOnly) float Duration = -1.f;
};
/**
 * FastArray 只发送变动条目；Owner 是本机回调目标，不随网络复制。
 */
USTRUCT()

struct GAS_DEMO_API FCC_ReplicatedBuffArray : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FCC_ReplicatedBuffItem> Items;
    UPROPERTY(NotReplicated) TObjectPtr<UCC_BuffComponent> Owner;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& Params)
    {
        return FastArrayDeltaSerialize<FCC_ReplicatedBuffItem, FCC_ReplicatedBuffArray>(Items, Params, *this);
    }

    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <> struct TStructOpsTypeTraits<FCC_ReplicatedBuffArray> : TStructOpsTypeTraitsBase2<FCC_ReplicatedBuffArray>
{
    enum
    {
        WithNetDeltaSerializer = true
    };
};
/**
 * 服务器拥有的单个 ActiveGE 运行时记录；Behaviors 强引用确保实例在回调期间可追踪。
 */
USTRUCT()

struct FCC_BuffInstance
{
    GENERATED_BODY()
    UPROPERTY() FCC_BuffInstanceView View;
    UPROPERTY() TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;
    bool bRemoveOnDeath = true;
    /**
     * 死亡保留 GE 时暂停规则，不继续让尸体参与钩子执行。
     */
    bool bSuspendedForDeath = false;
};
