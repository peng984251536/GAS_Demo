#include "Damage/CC_CombatFeedbackComponent.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "GameplayCues/CC_GameplayCue_DamageText.h"
/**
 * 配置事件驱动的反馈组件，不启用逐帧 Tick。
 */
UCC_CombatFeedbackComponent::UCC_CombatFeedbackComponent() { PrimaryComponentTick.bCanEverTick = false; }
/**
 * 先撤销旧订阅，再绑定新的 ASC 结算通知；传 nullptr 解除绑定。
 * @param ASC 待绑定的项目 ASC；反馈组件传 nullptr 表示解绑。
 */
void UCC_CombatFeedbackComponent::Initialize(UCC_AbilitySystemComponent* ASC)
{
    if (auto* Old = BoundASC.Get()) Old->OnDamageResolved.Remove(Handle);
    BoundASC = ASC;
    Handle = ASC ? ASC->OnDamageResolved.AddUObject(this, &ThisClass::HandleResult) : FDelegateHandle();
}
/**
 * 离开世界时解除结算订阅，再交给父类清理。
 * @param Reason 引擎传入的结束原因。
 */
void UCC_CombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Initialize(nullptr);
    Super::EndPlay(Reason);
}


/**
 * 从结算结果选择显示值，构造独立数字 Cue；不再次计算或修改生命。
 * @param Result 本次伤害/治疗的不可变结果副本，包含完整伤害、实际生命变化与归属。
 */
void UCC_CombatFeedbackComponent::HandleResult(const FCC_DamageResolved& Result)
{
    auto* ASC = BoundASC.Get();
    if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;
    // 治疗只显示真正恢复的血量；伤害显示口径由组件开关决定。
    const float Amount = Result.bHealing || bShowActualHealthDamage ? Result.HealthDamage : Result.ResolvedDamage;
    if (Amount <= 0.f) return;
    // 使用全新 Context，避免另一击或同步回调重入覆盖这次数字的类型与暴击标记。
    FGameplayEffectContextHandle Context(new FCC_GameplayEffectContext());
    Context.AddInstigator(Result.Source, Result.Source);
    auto* Typed = FCC_GameplayEffectContext::GetMutable(Context);
    Typed->DamageType = Result.DamageType;
    Typed->bCritical = Result.bCritical;
    FGameplayCueParameters Params;
    Params.EffectContext = Context;
    Params.Instigator = Result.Source;
    Params.EffectCauser = Result.Source;
    Params.Location = Result.TextLocation;
    Params.RawMagnitude = Result.bHealing ? -Amount : Amount;
    ASC->ExecuteGameplayCue(CCDamageCueTags::DamageText, Params);
}
