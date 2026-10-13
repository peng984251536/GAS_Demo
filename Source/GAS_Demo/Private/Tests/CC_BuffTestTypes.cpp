#include "Tests/CC_BuffTestTypes.h"
#include "Attribute/CC_AttributeSet.h"
#include "GameplayTags/CC_Tags.h"
#include "Damage/CC_DamageTypes.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AdditionalEffectsGameplayEffectComponent.h"

UCC_BuffTestConfiguration::UCC_BuffTestConfiguration()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(1.f);
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 2;
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
    bDenyOverflowApplication = true;
    auto& Attack = Modifiers.AddDefaulted_GetRef();
    Attack.Attribute = UCC_AttributeSet::GetAttackPowerAttribute();
    Attack.ModifierOp = EGameplayModOp::Additive;
    Attack.ModifierMagnitude = FScalableFloat(20.f);
    auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantSuperArmor"));
    GEComponents.Add(Tags);
    FInheritedTagContainer Changes;
    Changes.AddTag(CCTags::Status::SuperArmor);
    Tags->SetAndApplyTargetTagChanges(Changes);
    auto* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("NativeRequirements"));
    GEComponents.Add(Requirements);
    Requirements->ApplicationTagRequirements.IgnoreTags.AddTag(StandaloneDamageTags::DamageImmunity);
    Requirements->OngoingTagRequirements.IgnoreTags.AddTag(CCTags::Status::Invincible);
    auto* Additional = CreateDefaultSubobject<UAdditionalEffectsGameplayEffectComponent>(TEXT("NativeCompletionTrigger"));
    GEComponents.Add(Additional);
    Additional->OnCompletePrematurely.Add(UCC_BuffTestInstantEffect::StaticClass());
}

UCC_BuffTestPeriodicConfiguration::UCC_BuffTestPeriodicConfiguration()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(1.f);
    Period = FScalableFloat(0.25f);
    bExecutePeriodicEffectOnApplication = false;
    auto& Damage = Modifiers.AddDefaulted_GetRef();
    Damage.Attribute = UCC_AttributeSet::GetIncomingDamageAttribute();
    Damage.ModifierOp = EGameplayModOp::Additive;
    Damage.ModifierMagnitude = FScalableFloat(5.f);
}

UCC_BuffTestInstantConfiguration::UCC_BuffTestInstantConfiguration()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    auto& Healing = Modifiers.AddDefaulted_GetRef();
    Healing.Attribute = UCC_AttributeSet::GetIncomingHealingAttribute();
    Healing.ModifierOp = EGameplayModOp::Additive;
    Healing.ModifierMagnitude = FScalableFloat(10.f);
}

UCC_BuffTestEffect::UCC_BuffTestEffect() { BuffConfiguration = UCC_BuffTestConfiguration::StaticClass(); }
UCC_BuffTestOtherEffect::UCC_BuffTestOtherEffect() { BuffConfiguration = UCC_BuffTestConfiguration::StaticClass(); }
UCC_BuffTestPeriodicEffect::UCC_BuffTestPeriodicEffect() { BuffConfiguration = UCC_BuffTestPeriodicConfiguration::StaticClass(); }
UCC_BuffTestInstantEffect::UCC_BuffTestInstantEffect() { BuffConfiguration = UCC_BuffTestInstantConfiguration::StaticClass(); }
