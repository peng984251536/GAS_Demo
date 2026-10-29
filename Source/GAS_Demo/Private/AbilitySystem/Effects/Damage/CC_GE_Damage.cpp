#include "AbilitySystem/Effects/Damage/CC_GE_Damage.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Damage/CC_DamageExecution.h"
#include "Damage/CC_DamageTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UCC_GE_Damage::UCC_GE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UCC_DamageExecution::StaticClass();
	Executions.Add(Execution);

	// 免疫由 GAS 在应用前拦截；被拦截时没有属性修改，也不会触发 Cue 和伤害事件。
	auto* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("DamageRequirements"));
	Requirements->ApplicationTagRequirements.IgnoreTags.AddTag(CCDamageTags::Immunity);
	GEComponents.Add(Requirements);

	// 伤害数字：Cue 的数值取 Execution 输出到 Damage 的量；没有产生伤害时 GAS 不触发 Cue。
	FGameplayEffectCue DamageText(CCDamageTags::CueDamageText, 0.f, 1.f);
	DamageText.MagnitudeAttribute = UCC_AttributeSet::GetDamageAttribute();
	GameplayCues.Add(DamageText);
}
