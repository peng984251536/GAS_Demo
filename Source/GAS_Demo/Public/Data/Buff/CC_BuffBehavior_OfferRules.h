#pragma once
#include "Data/Buff/CC_BuffBehavior.h"
#include "CC_BuffBehavior_OfferRules.generated.h"

/**
 * 事后反应型规则：持有者造成有效伤害后，向存活的敌方目标施加配置的 GE。
 * 只消费通用 DamageDealt 事件，不参与伤害计算，也不判断持有者具体拥有哪些 Buff。
 * 魅惑、减速等差异由 EffectToApply 的配置决定，状态的实际行为由对应系统处理。
 *
 * Behavior 实例仅在服务器运行；启用时订阅事件，禁用时由基类自动解除订阅。
 * 当前未接入伤害递归深度保护，因此子 GE 必须是不造成伤害的效果，避免递归触发。
 */
UCLASS(EditInlineNew)
class GAS_DEMO_API UCC_BuffBehavior_ApplyEffectOnDamage : public UCC_BuffBehavior
{
    GENERATED_BODY()
public:
    /**
     * 命中后施加的状态 GE；模板只保存类配置，不保存目标或单次命中的运行时数据。
     * 子效果继承本次伤害的 Context 和父 Buff 的等级；其持续时间、叠层和免疫由 GE 自己定义。
     * 已施加的子效果独立存在，不随父 Buff 到期、抑制或移除而撤回。
     */
    UPROPERTY(EditDefaultsOnly, Category="Buff") TSubclassOf<UGameplayEffect> EffectToApply;
protected:
    /** 初次启用和抑制恢复时订阅 DamageDealt，使用基类工具保证订阅可对称回收。 */
    virtual void OnEnabled_Implementation() override;
    /**
     * 过滤无效命中后施加子效果；不额外造成伤害，也不修改共享配置。
     * @param Tag 实际收到的事件标签，只处理 Event.Combat.DamageDealt。
     * @param Event 扣血后事件：Instigator 为伤害来源，Target 为受击者，EventMagnitude 为实际扣血量。
     */
    virtual void OnGameplayEvent_Implementation(FGameplayTag Tag, const FGameplayEventData& Event) override;
};

/** Adds sequential volleys at the existing projectile decision point. */
UCLASS(EditInlineNew)
class GAS_DEMO_API UCC_BuffBehavior_RepeatVolley : public UCC_BuffBehavior
{
    GENERATED_BODY()
public:
    UCC_BuffBehavior_RepeatVolley();
    UPROPERTY(EditDefaultsOnly, Category="Buff", meta=(ClampMin="1", ClampMax="7")) int32 ExtraVolleys = 1;
    UPROPERTY(EditDefaultsOnly, Category="Buff", meta=(ClampMin="0.05")) float Interval = 0.18f;
protected:
    virtual void ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context) override;
};
