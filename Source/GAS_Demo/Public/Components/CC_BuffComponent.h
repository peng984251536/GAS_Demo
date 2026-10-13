#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffect.h"
#include "CC_BuffComponent.generated.h"

class UCC_BuffDefinition;
class UCC_BuffBehavior;
class UCC_GE_Buff;
class UAbilitySystemComponent;
class UAttributeSet;
class UCombatActionSet;
class APawn;

/**
 * 一个原生活动效果的 UI 数据快照，用于同步配置、效果类、层数和剩余时间。
 * 不参与数值计算；效果句柄与特殊玩法实例保存在角色组件中。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_ActiveBuffInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Buff") int32 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly, Category="Buff") TSubclassOf<UCC_BuffDefinition> Definition;
    UPROPERTY(BlueprintReadOnly, Category="Buff") TSubclassOf<UCC_GE_Buff> EffectClass;
    UPROPERTY(BlueprintReadOnly, Category="Buff") int32 StackCount = 1;
    UPROPERTY(BlueprintReadOnly, Category="Buff") bool bIsInhibited = false;
    /** -1 means infinite. */
    UPROPERTY(BlueprintReadOnly, Category="Buff") float EndServerTime = -1.f;
    UPROPERTY(BlueprintReadOnly, Category="Buff") float Duration = -1.f;
};

/**
 * 一个活动 GE 对应的角色本地实例数据，保存句柄、召唤物与特殊行为对象。
 * 不复制给 UI，不保存另一套修饰器或堆叠规则，也不写回共享配置。
 */
USTRUCT()
struct FCC_BuffRuntime
{
    GENERATED_BODY()
    int32 InstanceId = 0;
    FActiveGameplayEffectHandle Effect;
    bool bSpecialsStarted = false;
    UPROPERTY() TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;
    UPROPERTY() TArray<TObjectPtr<APawn>> Summons;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_BuffsChanged);

/**
 * 当前角色的 buff 访问组件，负责施加效果、监听 GAS 变化并同步 UI 信息。
 * 修饰器、触发条件、堆叠与周期计时由 ASC 管理，本组件不重复实现这些规则。
 * 召唤物和特殊行为对象属于当前角色，不能写回共享配置或效果默认对象。
 */
UCLASS(ClassGroup=(GAS), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_BuffComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCC_BuffComponent();
    /**
     * 在服务器向当前角色施加指定效果，修饰器与堆叠均按 GE 原生配置执行。
     * @param BuffEffect 已引用 buff 配置的效果类。
     * @param SourceASC 效果来源；为空时使用当前角色 ASC。
     * @param Level 效果等级，不覆盖配置中的持续时间或触发规则。
     * @return GAS 应用结果；Instant 效果成功时可能没有有效的活动句柄。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    FActiveGameplayEffectHandle ApplyBuff(TSubclassOf<UCC_GE_Buff> BuffEffect,
        UAbilitySystemComponent* SourceASC = nullptr, float Level = 1.f);
    /**
     * 根据 UI 实例编号移除对应活动 GE 的全部层数。
     * 真正的属性恢复、移除触发器和标签清理由 GAS 执行。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    bool RemoveBuff(int32 InstanceId);
    /**
     * 清理角色当前记录的活动 buff，供死亡、重生和销毁流程调用。
     * @param bDeathOnly 为 true 时只移除配置中允许死亡清除的效果。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    void ClearBuffs(bool bDeathOnly = false);
    /**
     * 返回 UI 快照，可在客户端读取；实际效果状态以 ASC 中的活动 GE 为准。
     */
    UFUNCTION(BlueprintPure, Category="Buff")
    TArray<FCC_ActiveBuffInfo> GetActiveBuffs() const { return ActiveBuffs; }
    /**
     * 汇总指定效果类的 GAS 实际层数，独立实例也会计入总数。
     */
    UFUNCTION(BlueprintPure, Category="Buff")
    int32 GetBuffStackCount(TSubclassOf<UCC_GE_Buff> BuffEffect) const;
    /**
     * 根据同步的服务器到期时间计算 UI 倒计时，不创建额外 buff 定时器。
     * @return 剩余秒数；永久效果返回 -1，已不存在的实例返回 0。
     */
    UFUNCTION(BlueprintPure, Category="Buff")
    float GetRemainingTime(int32 InstanceId) const;
    /**
     * 查找当前有效的攻击动作集覆盖，最新活动效果优先，被抑制的效果不参与覆盖。
     * 为空时由角色接口读取原角色配置，避免修改共享动作集配置。
     */
    UFUNCTION(BlueprintPure, Category="Buff")
    UCombatActionSet* GetAttackActionSetOverride() const;
    UPROPERTY(BlueprintAssignable, Category="Buff") FCC_BuffsChanged OnBuffsChanged;
    /**
     * 在角色 ASC 初始化后绑定活动效果与属性变化事件，并读取已有活动 buff。
     * 玩家和敌人的 ASC 所属对象不同，因此通过角色初始化通知接入。
     */
    UFUNCTION() void InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_ActiveBuffs) TArray<FCC_ActiveBuffInfo> ActiveBuffs;
    UPROPERTY(Transient) TArray<FCC_BuffRuntime> Runtime;
    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    int32 NextInstanceId = 1;
    float BaseWalkSpeed = 0.f;
    bool bBaseSpeedCaptured = false;
    bool bClearing = false;
    UFUNCTION() void OnRep_ActiveBuffs();
    void UnbindASC();
    void HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
    void HandleEffectRemoved(const FActiveGameplayEffect& Effect);
    void HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount);
    void HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration);
    void HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited);
    /**
     * 从活动 GE 读取层数、持续时间和抑制状态，更新可复制的 UI 快照。
     * 不计算效果数值，也不自行刷新堆叠、到期时间或执行周期。
     */
    void RefreshFromGAS(FActiveGameplayEffectHandle Handle);
    /**
     * 为当前活动效果创建角色专属的召唤物和特殊行为对象。
     * 原生堆叠增加只更新层数，不重复创建同一活动句柄的特殊行为。
     */
    void StartSpecials(int32 InstanceId);
    /**
     * 效果被抑制或移除时释放其特殊玩法状态，先清理记录再执行外部回调。
     * 可重复调用，避免回调重入导致同一资源被多次释放。
     */
    void StopSpecials(int32 InstanceId);
    void UpdateMovementSpeed(const FOnAttributeChangeData& Data);
    void NotifyChanged();
    float ServerTime() const;
};
