#include "Effects/Damage/CC_GE_Healing.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/CC_DamageTags.h"

UCC_GE_Healing::UCC_GE_Healing()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UCC_AttributeSet::GetHealingAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = CCDamageTags::HealingAmount;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);

	FGameplayEffectCue HealText(CCDamageTags::CueHealText, 0.f, 1.f);
	HealText.MagnitudeAttribute = UCC_AttributeSet::GetHealingAttribute();
	GameplayCues.Add(HealText);
}
