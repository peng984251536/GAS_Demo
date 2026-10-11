#include "Effects/Damage/CC_GE_Healing.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/CC_DamageTypes.h"

/**
 * 配置正数 SetByCaller 治疗量到 IncomingHealing 的中转 Modifier。
 */
UCC_GE_Healing::UCC_GE_Healing()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UCC_AttributeSet::GetIncomingHealingAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat Magnitude;
    Magnitude.DataTag = StandaloneDamageTags::Healing;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
