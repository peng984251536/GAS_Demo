#include "Tests/CC_BuffTestTypes.h"
#include "Attribute/CC_AttributeSet.h"
#include "GameplayTags/CC_Tags.h"
#include "Damage/CC_DamageTypes.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AdditionalEffectsGameplayEffectComponent.h"

UCC_BuffTestEffect::UCC_BuffTestEffect()
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
    auto* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    GEComponents.Add(Rules);
    Rules->Behaviors.Add(CreateDefaultSubobject<UCC_BuffTestBehavior>(TEXT("LifecycleBehavior")));
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI")));
}
UCC_BuffTestPeriodicEffect::UCC_BuffTestPeriodicEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(1.f);
    Period = FScalableFloat(0.25f);
    bExecutePeriodicEffectOnApplication = false;
    auto& Damage = Modifiers.AddDefaulted_GetRef();
    Damage.Attribute = UCC_AttributeSet::GetIncomingDamageAttribute();
    Damage.ModifierOp = EGameplayModOp::Additive;
    Damage.ModifierMagnitude = FScalableFloat(5.f);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
}
UCC_BuffTestInstantEffect::UCC_BuffTestInstantEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    auto& Healing = Modifiers.AddDefaulted_GetRef();
    Healing.Attribute = UCC_AttributeSet::GetIncomingHealingAttribute();
    Healing.ModifierOp = EGameplayModOp::Additive;
    Healing.ModifierMagnitude = FScalableFloat(10.f);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI")));
}
UCC_BuffTestRetainedEffect::UCC_BuffTestRetainedEffect()
{
    FindOrAddComponent<UCC_BuffRulesComponent>().bRemoveOnDeath = false;
}
UCC_BuffTestBehavior::UCC_BuffTestBehavior()
{
    HandledHooks.AddTag(CCTags::CCAbilityTrigger::BowShoot);
}
void UCC_BuffTestBehavior::OnEnabled_Implementation()
{
    ++Enables;
    ListenGameplayEvent(CCTags::CCAbilityTrigger::BowShoot);
    StartTimer(0.1f, true, FTimerDelegate::CreateWeakLambda(this, [this] { ++Ticks; }));
    EnabledActor = SpawnOwnedActor(AActor::StaticClass(), FTransform::Identity, ECC_BuffResourceLifetime::WhileEnabled);
    if (!InstanceActor.IsValid()) InstanceActor = SpawnOwnedActor(AActor::StaticClass(), FTransform::Identity, ECC_BuffResourceLifetime::Instance);
}
void UCC_BuffTestBehavior::ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context)
{
    if (bConsumeOnHook) ConsumeStacks(GetContext().StackCount);
}
