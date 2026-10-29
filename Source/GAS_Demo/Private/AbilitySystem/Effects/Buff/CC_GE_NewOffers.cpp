#include "AbilitySystem/Effects/Buff/CC_GE_NewOffers.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffBehavior_OfferRules.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/CC_Tags.h"
#include "Actors/Projectiles/CC_ProjectileInteractions.h"

/** 永久强攻：配置攻击力 +20 的可叠加修饰器，以及倒地保留策略。 */
UCC_GE_AttackPermanent::UCC_GE_AttackPermanent()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1000;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("永久强攻"));
    UI->Description = FText::FromString(TEXT("永久增加攻击力 20；重复获得可叠加，倒地保留。"));
    GEComponents.Add(UI);
    auto* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    Rules->bRemoveOnDeath = false;
    GEComponents.Add(Rules);
    FGameplayModifierInfo Mod;
    Mod.Attribute = UCC_AttributeSet::GetAttackPowerAttribute();
    Mod.ModifierOp = EGameplayModOp::Additive;
    Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(20.f));
    Modifiers.Add(Mod);
}

/** 生命再生：配置五秒周期治疗，治疗量读取目标最大生命值，不在获得瞬间执行。 */
UCC_GE_RegenPercent::UCC_GE_RegenPercent()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));
    Period = FScalableFloat(1.f);
    bExecutePeriodicEffectOnApplication = false;
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("生命再生"));
    UI->Description = FText::FromString(TEXT("每秒恢复最大生命值的 1%，持续 5 秒。"));
    GEComponents.Add(UI);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
    FAttributeBasedFloat Amount;
    Amount.Coefficient = FScalableFloat(0.01f);
    Amount.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
        UCC_AttributeSet::GetMaxHealthAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
    FGameplayModifierInfo Mod;
    Mod.Attribute = UCC_AttributeSet::GetHealingAttribute();
    Mod.ModifierOp = EGameplayModOp::Additive;
    Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
    Modifiers.Add(Mod);
}

/** 十秒霸体：只授予状态标签，不授予伤害免疫。 */
UCC_GE_ArmorTen::UCC_GE_ArmorTen()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("霸体"));
    UI->Description = FText::FromString(TEXT("获得霸体 10 秒，免受受击动作打断，仍承受伤害。"));
    GEComponents.Add(UI);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
    auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("Status"));
    FInheritedTagContainer Changes;
    Changes.Added.AddTag(CCTags::Status::SuperArmor);
    Tags->SetAndApplyTargetTagChanges(Changes);
    GEComponents.Add(Tags);
}

/** 敌人身上的魅惑子效果：配置十秒状态标签，由阵营和 AI 系统响应。 */
UCC_GE_CharmTen::UCC_GE_CharmTen()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
    StackingType = EGameplayEffectStackingType::AggregateBySource;
    StackLimitCount = 1;
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
    auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("Status"));
    FInheritedTagContainer Changes;
    Changes.Added.AddTag(CCTags::Status::Charmed);
    Tags->SetAndApplyTargetTagChanges(Changes);
    GEComponents.Add(Tags);
}

/** 持有者身上的十秒魅惑攻击：把魅惑子 GE 配置给通用伤害后反应规则。 */
UCC_GE_CharmAttackTen::UCC_GE_CharmAttackTen()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("魅惑攻击"));
    UI->Description = FText::FromString(TEXT("10 秒内，攻击命中使存活敌人被魅惑 10 秒。"));
    GEComponents.Add(UI);
    auto* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    auto* Rule = NewObject<UCC_BuffBehavior_ApplyEffectOnDamage>(Rules, TEXT("OnDamage"), RF_Public | RF_ArchetypeObject);
    Rule->EffectToApply = UCC_GE_CharmTen::StaticClass();
    Rules->Behaviors.Add(Rule);
    GEComponents.Add(Rules);
}

/** 永久分裂箭：沿用父类的分裂规则，将持续时间改为无限，并保留倒地后的强化。 */
UCC_GE_SplitPermanent::UCC_GE_SplitPermanent()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    StackLimitCount = 1;
    FindOrAddComponent<UCC_BuffRulesComponent>().bRemoveOnDeath = false;
    auto* UI = &FindOrAddComponent<UCC_BuffUIData>();
    UI->DisplayName = FText::FromString(TEXT("分裂箭"));
    UI->Description = FText::FromString(TEXT("永久强化弓箭：每轮射出 3 支箭，散射 30°，单箭伤害 60%。可与连射组合。"));
}

/** 永久连续两箭：配置连续轮次规则模板，实际延迟发射由弓箭系统执行。 */
UCC_GE_DoubleArrowPermanent::UCC_GE_DoubleArrowPermanent()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("连续两箭"));
    UI->Description = FText::FromString(TEXT("永久强化弓箭：每次攻击连续发射两轮，间隔 0.18 秒。可与分裂箭组合。"));
    GEComponents.Add(UI);
    auto* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    Rules->bRemoveOnDeath = false;
    Rules->Behaviors.Add(NewObject<UCC_BuffBehavior_RepeatVolley>(Rules, TEXT("Repeat"), RF_Public | RF_ArchetypeObject));
    GEComponents.Add(Rules);
}

/** 反弹箭资格：仅授予标签，资格撤回由 GAS 管理，飞行中的反弹箭不随 Buff 移除。 */
UCC_GE_DeflectArrowPermanent::UCC_GE_DeflectArrowPermanent()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    auto* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = FText::FromString(TEXT("挥击反弹"));
    UI->Description = FText::FromString(TEXT("永久获得反弹资格：挥砍有效帧将范围内敌方箭弹回，每支箭最多反弹一次。"));
    GEComponents.Add(UI);
    auto* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    Rules->bRemoveOnDeath = false;
    GEComponents.Add(Rules);
    auto* Status = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeflectQualification"));
    FInheritedTagContainer Tags;
    Tags.Added.AddTag(CCProjectileTags::CanDeflect);
    Status->SetAndApplyTargetTagChanges(Tags);
    GEComponents.Add(Status);
}
