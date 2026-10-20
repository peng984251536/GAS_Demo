#include "Damage/CC_DamageExecution.h"
#include "Damage/CC_DamageFormula.h"
#include "Damage/CC_DamageTags.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogCCDamage, Log, All);

namespace
{
	/** 捕获定义集中在这里：攻击方快照（true），目标实时读取（false）。 */
	struct FCaptures
	{
		FGameplayEffectAttributeCaptureDefinition Attack{UCC_AttributeSet::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true};
		FGameplayEffectAttributeCaptureDefinition Bonus{UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), EGameplayEffectAttributeCaptureSource::Source, true};
		FGameplayEffectAttributeCaptureDefinition CritChance{UCC_AttributeSet::GetCriticalChanceAttribute(), EGameplayEffectAttributeCaptureSource::Source, true};
		FGameplayEffectAttributeCaptureDefinition CritMultiplier{UCC_AttributeSet::GetCriticalMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Source, true};
		FGameplayEffectAttributeCaptureDefinition Defense{UCC_AttributeSet::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, false};
		FGameplayEffectAttributeCaptureDefinition Incoming{UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, false};
		FGameplayEffectAttributeCaptureDefinition Health{UCC_AttributeSet::GetHealthAttribute(), EGameplayEffectAttributeCaptureSource::Target, false};
	};
	const FCaptures& Captures()
	{
		static const FCaptures Instance;
		return Instance;
	}
}

UCC_DamageExecution::UCC_DamageExecution()
{
	const FCaptures& C = Captures();
	RelevantAttributesToCapture.Append({C.Attack, C.Bonus, C.CritChance, C.CritMultiplier, C.Defense, C.Incoming, C.Health});
}

void UCC_DamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
	FGameplayEffectCustomExecutionOutput& Output) const
{
#if WITH_SERVER_CODE
	const FCaptures& C = Captures();
	const FGameplayEffectSpec& Spec = Params.GetOwningSpec();
	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	auto Read = [&](const FGameplayEffectAttributeCaptureDefinition& Capture, float Default)
	{
		float Value = Default;
		Params.AttemptCalculateCapturedAttributeMagnitude(Capture, Evaluation, Value);
		return Value;
	};

	// 已死亡的目标不再结算：不输出修改，GAS 也就不会触发 Cue 和伤害事件。
	if (Read(C.Health, 0.f) <= 0.f) return;

	// 暴击写进本次执行自己的 Context 副本，Cue 和 GameplayEvent 读到的都是这一份。
	if (FGameplayEffectSpec* MutableSpec = Params.GetOwningSpecForPreExecuteMod()) MutableSpec->DuplicateEffectContext();
	FGameplayEffectContextHandle Context = Spec.GetContext();
	FCC_GameplayEffectContext* Typed = FCC_GameplayEffectContext::GetMutable(Context);
	bool bCritical = false;
	if (Typed)
	{
		bCritical = Typed->CriticalPolicy == ECC_CriticalPolicy::Forced
			|| (Typed->CriticalPolicy == ECC_CriticalPolicy::Random && FMath::FRand() < Read(C.CritChance, 0.f));
		Typed->bCritical = bCritical;
	}

	CCDamageFormula::FInput Input;
	Input.AttackPower = Read(C.Attack, 0.f);
	Input.OutgoingBonus = Read(C.Bonus, 0.f);
	Input.CriticalMultiplier = bCritical ? Read(C.CritMultiplier, 1.f) : 1.0;
	Input.Defense = Read(C.Defense, 0.f);
	Input.IncomingMultiplier = Read(C.Incoming, 1.f);
	Input.SkillScale = Spec.GetSetByCallerMagnitude(CCDamageTags::SkillScale, false, 1.f);
	Input.FlatDamage = Spec.GetSetByCallerMagnitude(CCDamageTags::FlatDamage, false, 0.f);
	Input.DamageMultiplier = Spec.GetSetByCallerMagnitude(CCDamageTags::DamageMultiplier, false, 1.f);

	double Damage = 0.0;
	if (!CCDamageFormula::Calculate(Input, Damage) || Damage > MAX_flt)
	{
		UE_LOG(LogCCDamage, Warning, TEXT("伤害计算输入非法或溢出，已跳过：GE=%s Attack=%.2f Scale=%.2f Flat=%.2f Mult=%.2f"),
			*GetNameSafe(Spec.Def), Input.AttackPower, Input.SkillScale, Input.FlatDamage, Input.DamageMultiplier);
		return;
	}
	if (Damage <= 0.0) return;

	// 一次执行就是一击，不乘 GE 的 StackCount。
	Output.MarkStackCountHandledManually();
	Output.AddOutputModifier(FGameplayModifierEvaluatedData(UCC_AttributeSet::GetDamageAttribute(),
		EGameplayModOp::Additive, static_cast<float>(Damage)));
#endif
}
