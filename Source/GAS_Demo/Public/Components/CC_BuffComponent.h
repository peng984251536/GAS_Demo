#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Buff/CC_BuffTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "CC_BuffComponent.generated.h"

class UAttributeSet;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_BuffsChanged);

/**
 * 每个角色一份的 Buff 运行时管理器：连接 GAS、服务器规则实例与客户端展示快照。
 * 属性修饰、持续时间、叠层、标签和能力授予交给 GAS；组件不包含具体 Buff 的玩法分支。
 * 服务器根据 ActiveGE 事件维护实例和钩子，客户端只接收 FastArray 并通知 UI。
 */
UCLASS(ClassGroup = (GAS), meta = (BlueprintSpawnableComponent))

class GAS_DEMO_API UCC_BuffComponent : public UActorComponent
{
    GENERATED_BODY()

  public:
    UCC_BuffComponent();
    /**
	 * 施加一个具有 UI 或 Rules 组件的 GE；同类效果是否叠层、刷新由 GE 自身配置决定。
	 * @param EffectClass Buff 的 GE 类，不能是抽象类。
	 * @param SourceASC 施加者 ASC；为空时使用当前角色自己的 ASC。
	 * @param Level GE 等级，必须是有限正数。
	 * @return GAS 的施加结果；判断成功使用 WasSuccessfullyApplied，Instant GE 不一定有活动句柄。
	 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff")
    FActiveGameplayEffectHandle ApplyBuff(TSubclassOf<UGameplayEffect> EffectClass,
                                          UAbilitySystemComponent* SourceASC = nullptr, float Level = 1.f);

    /**
	 * 带运行时参数的原生入口；只修改本次 Spec，不修改共享 GE 或 Behavior 模板。
	 * @param EffectClass 本次施加的 GE 类。
	 * @param SourceASC 来源 ASC；为空时使用自身 ASC。
	 * @param Level 本次 GE 的正数等级。
	 * @param SetByCaller 参数标签到数值的映射；消费这些参数的是 GE 或 Behavior。
	 * @return 本次 GAS 施加结果。
	 */
    FActiveGameplayEffectHandle ApplyBuffWithParameters(TSubclassOf<UGameplayEffect> EffectClass,
                                                        UAbilitySystemComponent* SourceASC, float Level,
                                                        const TMap<FGameplayTag, float>& SetByCaller);

    /**
	 * 按展示实例号移除完整 GE；GAS 移除回调统一负责退出规则和更新 UI。
	 * @param InstanceId 当前组件分配的实例号，不是 GE 类编号或数组下标。
	 * @return 找到对应活动 GE 并成功提交移除时返回 true。
	 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff")
    bool RemoveBuff(int32 InstanceId);

    /**
	 * 按资产标签与授予标签匹配后，从旧到新驱散完整 Buff，不是逐层扣减。
	 * @param Query 标签查询；空查询匹配全部。
	 * @param MaxCount 最多移除的实例数；-1 为不限，0 为不移除。
	 * @return 实际成功移除的实例数量。
	 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff")
    int32 RemoveBuffsByQuery(const FGameplayTagQuery& Query, int32 MaxCount = -1);

    /**
	 * 从展示快照累计具有指定标签的 GE 层数；多个实例的层数会相加。
	 * @param Tag 要统计的标签。
	 * @param bIncludeInhibited 是否把被抑制但尚未移除的效果计入。
	 * @return 符合条件的总层数；客户端结果以最近收到的复制快照为准。
	 */
    UFUNCTION(BlueprintPure, Category = "Buff")
    int32 GetStackCountByTag(FGameplayTag Tag, bool bIncludeInhibited = false) const;

    /**
	 * 批量结束 Buff；死亡时允许配置保留 GE 的实例进入暂停，而其他清理原因完整结束。
	 * @param Reason 区分死亡、重置、ASC 更换和关卡退出，传给规则结束回调。
	 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff")
    void ClearBuffs(ECC_BuffEndReason Reason = ECC_BuffEndReason::Reset);

    /**
	 * 复活后恢复死亡时被挂起的 Buff（bRemoveOnDeath = false 的实例），与 ClearBuffs(OwnerDeath) 对称：
	 * 清除 bSuspendedForDeath，按 GAS 当前状态重新启用钩子和 Behavior（仍被抑制的保持禁用）。
	 * 只在服务器执行；没有挂起实例时什么也不做。
	 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff")
    void ResumeAfterRevive();

    /**
	 * 返回客户端可展示数据的副本，不暴露服务器的 Behavior 实例或可修改的 GAS 存储。
	 * @return 当前已记录的 Buff 展示条目。
	 */
    UFUNCTION(BlueprintPure, Category = "Buff")

    TArray<FCC_ReplicatedBuffItem> GetActiveBuffs() const
    {
        return ReplicatedBuffs.Items;
    }

    /**
	 * 用服务器时钟和已复制到期时刻计算倒计时，不为每个 Buff 创建本地计时器。
	 * @param InstanceId 要查询的实例号。
	 * @return 无限时长返回 -1；不存在或已经到期返回 0；其他情况返回剩余秒数。
	 */
    UFUNCTION(BlueprintPure, Category = "Buff")
    float GetRemainingTime(int32 InstanceId) const;

    UPROPERTY(BlueprintAssignable, Category = "Buff")
    FCC_BuffsChanged OnBuffsChanged;

    /**
	 * 切换或首次绑定角色 ASC；先清理旧实例和委托，再接管新 ASC 的现有效果。
	 * @param ASC 新的角色 ASC；为空表示解除绑定。
	 * @param Attributes 角色初始化通知携带的属性集，当前实现不缓存该参数。
	 */
    UFUNCTION()
    void InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes);
    /**
	 * 仅服务器执行生效规则；排序为优先级、实例号、模板索引，均按升序。
	 * @param Hook 玩法系统暴露的通用决策点标签。
	 * @param Context 此次操作的上下文；规则可改字段，不能替换结构体类型。
	 */
    void RunHook(FGameplayTag Hook, FInstancedStruct& Context);

    /**
	 * 将明确的上下文类型适配到统一钩子入口，完成后把修改结果拷回调用者。
	 * @param Hook 本次决策点标签。
	 * @param Context 调用者栈上的结构体；只在本次调用期间参与规则修改。
	 */
    template <typename T> void RunHook(FGameplayTag Hook, T& Context)
    {
        FInstancedStruct Value = FInstancedStruct::Make<T>(Context);
        RunHook(Hook, Value);
        if (const T* Result = Value.GetPtr<T>())
        {
            Context = *Result;
        }
    }

    /**
	 * 扣减指定 GE 的层数；是否触发完整结束由 GAS 按剩余层数决定。
	 * @param Handle 当前组件管理的活动 GE。
	 * @param Count 要消耗的正数层数。
	 * @return GAS 是否接受本次移除层数的操作。
	 */
    bool ConsumeStacks(FActiveGameplayEffectHandle Handle, int32 Count);
    void NotifyReplicatedChange();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

  private:
    /**
	 * 钩子索引只保存弱 Behavior 引用和排序键；真正拥有实例的是 Instances。
	 */
    struct FHookEntry
    {
        TWeakObjectPtr<UCC_BuffBehavior> Behavior;
        FActiveGameplayEffectHandle Handle;
        int32 Priority = 0;
        int32 InstanceId = 0;
        int32 BehaviorIndex = 0;
    };

    /**
	 * 面向客户端的最小展示数据；不复制 Behavior、资源句柄或服务器回调。
	 */
    UPROPERTY(Replicated)
    FCC_ReplicatedBuffArray ReplicatedBuffs;
    /**
	 * 服务器的 GE 句柄到运行时实例表；同一 GE 增减层数时复用实例。
	 */
    UPROPERTY(Transient)
    TMap<FActiveGameplayEffectHandle, FCC_BuffInstance> Instances;
    /**
	 * 只索引当前启用的规则，避免每次放箭都遍历全部 Buff。
	 */
    TMap<FGameplayTag, TArray<FHookEntry>> HookIndex;
    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    /**
	 * 主动清理时暂存结束原因，让同步的 GAS 移除回调使用同一原因。
	 */
    TMap<FActiveGameplayEffectHandle, ECC_BuffEndReason> RemovalReasons;
    int32 NextInstanceId = 1;
    int32 HookDepth = 0;
    /**
	 * 批量清理的重入保护；结束回调不能在同一过程中重新施加 Buff。
	 */
    bool bClearing = false;
    bool bEnded = false;
    /**
	 * 既有移速适配保留基础速度，只按 GAS 的 MoveSpeedMultiplier 重新计算。
	 */
    float BaseWalkSpeed = 0.f;
    void UpdateMovementSpeed(const FOnAttributeChangeData& Data);
    void UnbindASC();
    void HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec,
                           FActiveGameplayEffectHandle Handle);
    void HandleEffectRemoved(const FActiveGameplayEffect& Effect);
    void HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount);
    void HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration);
    void HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited);
    void RefreshFromGAS(FActiveGameplayEffectHandle Handle, bool bRefreshed = false);
    void FinishInstance(FActiveGameplayEffectHandle Handle, ECC_BuffEndReason Reason);
    void SetHooks(FActiveGameplayEffectHandle Handle, bool bEnabled);
    bool CanApply() const;
    void NotifyChanged();
    float ServerTime() const;
};
