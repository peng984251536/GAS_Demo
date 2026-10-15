#pragma once
#include "CoreMinimal.h"
#include "Data/Buff/CC_BuffTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "TimerManager.h"
#include "CC_BuffBehavior.generated.h"

/**
 * 规则层扩展点：GE 中保存只读模板，BuffComponent 为每个 ActiveGE 复制服务器实例。
 * 状态顺序为 Uninitialized → Disabled ⇄ Enabled → Finished；结束后的实例不允许重新启用。
 * 子类只实现规则，并通过基类工具登记资源，避免抑制、死亡或驱散后残留订阅和生成物。
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)

class GAS_DEMO_API UCC_BuffBehavior : public UObject
{
    GENERATED_BODY()
  public:
    /**
     * 声明参与的通用钩子标签；组件仅把启用实例登记到这些标签的索引。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Hooks")
    FGameplayTagContainer HandledHooks;
    /**
     * 钩子优先级越小越先执行；相同优先级按实例号和模板索引保证稳定顺序。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Hooks")
    int32 HookPriority = 0;
    virtual UWorld* GetWorld() const override;

    bool IsEnabled() const
    {
        return State == ECC_BuffBehaviorState::Enabled;
    }

    const FCC_BuffInstanceView& GetContext() const
    {
        return View;
    }

  protected:
    /**
     * 每个运行时实例仅调用一次，且可能发生在 GE 已被抑制时。
     * 需要随启用收回的资源应在 OnEnabled 创建，不应在这里启动。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnInitialized();

    virtual void OnInitialized_Implementation()
    {
    }

    /**
     * 初次生效或抑制恢复时调用；此时实例已经是 Enabled，可使用自动回收工具。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnEnabled();

    virtual void OnEnabled_Implementation()
    {
    }

    /**
     * 停止规则后调用，随后基类移除事件订阅、计时器及 WhileEnabled 资源。
     * @param Reason 说明是抑制、持有者死亡，还是最终结束。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnDisabled(ECC_BuffDisableReason Reason);

    virtual void OnDisabled_Implementation(ECC_BuffDisableReason Reason)
    {
    }

    /**
     * 更新快照之后通知层数变化；不重新创建实例，也不重新执行初始化。
     * @param OldCount 变化之前的 GE 层数。
     * @param NewCount 更新后的 GE 层数，可直接从 GetContext 读取。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnStackChanged(int32 OldCount, int32 NewCount);

    virtual void OnStackChanged_Implementation(int32 OldCount, int32 NewCount)
    {
    }

    /**
     * 持续时间改变时通知同一实例；回调不代表新叠一层，也不重新启动全部资源。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnRefreshed();

    virtual void OnRefreshed_Implementation()
    {
    }

    /**
     * 实例最终结束时最多调用一次；状态已经是 Finished，不能再登记新资源。
     * @param Reason GE 被移除或组件清理的最终原因。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff") void OnShutdown(ECC_BuffEndReason Reason);

    virtual void OnShutdown_Implementation(ECC_BuffEndReason Reason)
    {
    }

    /**
     * 事前修改型规则：只修改本次上下文，不能保存调用者结构体的地址。
     * @param Hook 当前决策点标签。
     * @param Context 可修改字段的合并方式由该上下文类型定义。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff|Hooks")
    void ModifyHook(FGameplayTag Hook, UPARAM(ref) FInstancedStruct& Context);

    virtual void ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context)
    {
    }

    /**
     * 事后反应型规则：仅在启用且仍订阅此事件时转发。
     * @param Tag 实际收到的事件标签。
     * @param Event 事件数据；由此派生的子 GE 会继承事件 Context 的副本。
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Buff|Events")
    void OnGameplayEvent(FGameplayTag Tag, const FGameplayEventData& Event);

    virtual void OnGameplayEvent_Implementation(FGameplayTag Tag, const FGameplayEventData& Event)
    {
    }

    UFUNCTION(BlueprintPure, Category = "Buff") FCC_BuffInstanceView GetInstanceView() const
    {
        return View;
    }

    /**
     * 向目标 ASC 订阅事件；同标签去重，禁用时由基类自动解除。
     * @param Tag 要监听的 GameplayEvent 标签。
     */
    UFUNCTION(BlueprintCallable, Category = "Buff|Resources") void ListenGameplayEvent(FGameplayTag Tag);
    /**
     * 创建仅在 Enabled 阶段有效的服务器计时器，Disable 和 Shutdown 都会清理。
     * @param Interval 正数间隔，单位秒。
     * @param bLoop 是否循环执行。
     * @param Callback 计时回调，不应捕获生命周期更短的裸指针。
     * @return 创建成功的计时器句柄；参数无效或未启用时返回空句柄。
     */
    FTimerHandle StartTimer(float Interval, bool bLoop, const FTimerDelegate& Callback);
    /**
     * 生成资源并按生命周期登记；Independent 不归父 Buff 回收。
     * @param Class 要生成的 Actor 类。
     * @param Transform 世界空间生成变换。
     * @param Lifetime 禁用时回收、实例结束回收，或完全独立。
     * @return 生成的 Actor；生成失败时为空，调用者仍应检查有效性。
     */
    UFUNCTION(BlueprintCallable, Category = "Buff|Resources")
    AActor* SpawnOwnedActor(TSubclassOf<AActor> Class, const FTransform& Transform,
                            ECC_BuffResourceLifetime Lifetime = ECC_BuffResourceLifetime::Instance);
    /**
     * 施加子效果；受父实例管理的 GE 不允许堆叠，避免一个句柄被多个父 Buff 共同拥有。
     * @param Target 接收子 GE 的 ASC。
     * @param Effect 子 GE 的类。
     * @param Level 有限正数等级。
     * @param Lifetime 默认 Independent，父 Buff 结束不会撤回独立的子效果。
     * @return 子 GE 的应用结果；失败时返回空句柄。
     */
    UFUNCTION(BlueprintCallable, Category = "Buff|Resources")
    FActiveGameplayEffectHandle
        ApplyChildEffect(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> Effect, float Level = 1.f,
                         ECC_BuffResourceLifetime Lifetime = ECC_BuffResourceLifetime::Independent);
    /**
     * 消耗自身 GE 的层数，可能同步触发自己的禁用与结束回调。
     * @param Count 正数层数；调用后不得假定实例仍然生效。
     * @return 是否接受消耗操作。
     */
    UFUNCTION(BlueprintCallable, Category = "Buff") bool ConsumeStacks(int32 Count = 1);

  private:
    friend class UCC_BuffComponent;
    void Initialize(UCC_BuffComponent* Component, const FCC_BuffInstanceView& InView);
    void UpdateView(const FCC_BuffInstanceView& InView, bool bRefreshed);
    void Enable();
    void Disable(ECC_BuffDisableReason Reason);
    void Shutdown(ECC_BuffEndReason Reason);
    void ReceiveGameplayEvent(FGameplayTag Tag, const FGameplayEventData* Event);
    /**
     * 先摘下资源记录再逐项释放，以应对销毁 Actor 或移除 GE 导致的同步重入。
     * @param bFinal 为 true 时回收全部受管资源；否则保留 Instance 资源。
     */
    void ReleaseResources(bool bFinal);
    UPROPERTY(Transient) FCC_BuffInstanceView View;
    UPROPERTY(Transient) TWeakObjectPtr<UCC_BuffComponent> Component;
    UPROPERTY(Transient) ECC_BuffBehaviorState State = ECC_BuffBehaviorState::Uninitialized;

    struct FEventSubscription
    {
        TWeakObjectPtr<UAbilitySystemComponent> ASC;
        FGameplayTagContainer Tags;
        FDelegateHandle Handle;
    };

    struct FOwnedActor
    {
        TWeakObjectPtr<AActor> Actor;
        ECC_BuffResourceLifetime Lifetime;
    };

    struct FChildEffect
    {
        TWeakObjectPtr<UAbilitySystemComponent> ASC;
        FActiveGameplayEffectHandle Handle;
        ECC_BuffResourceLifetime Lifetime;
    };

    TArray<FEventSubscription> Subscriptions;
    TArray<FTimerHandle> Timers;
    TArray<FOwnedActor> Actors;
    TArray<FChildEffect> Effects;
    /**
     * 仅在处理事件的调用栈内临时保存 Context；嵌套事件结束后自动恢复外层数据。
     */
    FGameplayEffectContextHandle CurrentEventContext;
};
