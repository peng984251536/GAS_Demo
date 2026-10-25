#include "Data/Buff/CC_BuffBehavior_OfferRules.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Damage/CC_DamageTags.h"
#include "Utils/Team/CC_TeamStatics.h"
#include "NativeGameplayTags.h"

// 持续伤害来源标签：命中附加状态规则排除 DoT，避免每次周期扣血都再次触发。
UE_DEFINE_GAMEPLAY_TAG_STATIC(CCOfferDoTDamage, "Damage.Source.DoT");

void UCC_BuffBehavior_ApplyEffectOnDamage::OnEnabled_Implementation()
{
    // 订阅当前持有者 ASC 的事件。基类在禁用、死亡挂起或结束时自动解除，恢复时重新订阅。
    ListenGameplayEvent(CCDamageTags::EventDamageDealt);
}

void UCC_BuffBehavior_ApplyEffectOnDamage::OnGameplayEvent_Implementation(
    FGameplayTag Tag, const FGameplayEventData& Event)
{
    const auto* Target = Cast<ACC_BaseCharacter>(Event.Target.Get());
    // 只接受实际扣血的非 DoT 命中，且目标必须是存活的敌方角色；敌我统一通过 TeamStatics 查询。
    // DamageDealt 早于 OnOutOfHealth 广播，致命命中时 IsAlive 可能尚未更新，因此还需检查剩余生命。
    if (Tag != CCDamageTags::EventDamageDealt ||
        !EffectToApply ||
        Event.EventMagnitude <= 0.f ||
        Event.InstigatorTags.HasTag(CCOfferDoTDamage) ||
        !Target || !Target->IsAlive() ||
        !UCC_TeamStatics::IsHostile(Event.Instigator.Get(), Target) ||
        !Target->GetAttributeSet() ||
        Target->GetAttributeSet()->GetHealth() < UCC_AttributeSet::OutOfHealthThreshold)
        return;
    // 通过基类施加：继承当前事件 Context（保留伤害来源，供魅惑等状态确定施加者）。
    // 默认资源生命周期为 Independent：父 Buff 结束只停止后续触发，不移除已经命中的状态。
    ApplyChildEffect(Target->GetAbilitySystemComponent(), EffectToApply, GetContext().Level);
}

UCC_BuffBehavior_RepeatVolley::UCC_BuffBehavior_RepeatVolley()
{
    HandledHooks.AddTag(CCBuffTags::ProjectileFire);
}

void UCC_BuffBehavior_RepeatVolley::ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context)
{
    auto* Fire = Context.GetMutablePtr<FCC_ProjectileFireContext>();
    if (Hook != CCBuffTags::ProjectileFire || !Fire || !FMath::IsFinite(Interval)) return;
    Fire->VolleyCount = FMath::Clamp(Fire->VolleyCount + FMath::Clamp(ExtraVolleys, 1, 7) *
        FMath::Clamp(GetContext().StackCount, 1, 8), 1, 8);
    Fire->VolleyInterval = FMath::Max(Fire->VolleyInterval, FMath::Clamp(Interval, 0.05f, 2.f));
}
