#include "Damage/CC_DamageExecution.h"
#include "Damage/CC_DamageTypes.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Damage/StandaloneDamageCalculator.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogStandaloneDamage, Log, All);

namespace StandaloneDamageTags
{
	UE_DEFINE_GAMEPLAY_TAG(FlatDamage, "Data.StandaloneDamage.FlatDamage");
	UE_DEFINE_GAMEPLAY_TAG(SkillScale, "Data.StandaloneDamage.SkillScale");
	UE_DEFINE_GAMEPLAY_TAG(DistanceMultiplier, "Data.StandaloneDamage.DistanceMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(HitMultiplier, "Data.StandaloneDamage.HitMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(Healing, "Data.StandaloneDamage.Healing");
	UE_DEFINE_GAMEPLAY_TAG(DamageImmunity, "Status.DamageImmunity");
}


/**
 * 一次登记项目所需的六个属性捕获，不维护动态属性绑定或单次命中的共享状态。
 */
UCC_DamageExecution::UCC_DamageExecution()
	: AttackCapture(UCC_AttributeSet::GetAttackPowerAttribute(),
		EGameplayEffectAttributeCaptureSource::Source, true)
	  , DefenseCapture(UCC_AttributeSet::GetDefenseAttribute(),
	  	EGameplayEffectAttributeCaptureSource::Target, false)
	  , BonusCapture(UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), 
		EGameplayEffectAttributeCaptureSource::Source,true)
	  , IncomingCapture(UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(),
		EGameplayEffectAttributeCaptureSource::Target, false)
	  , ChanceCapture(UCC_AttributeSet::GetCriticalChanceAttribute(),
		EGameplayEffectAttributeCaptureSource::Source,true)
	  , CriticalCapture(UCC_AttributeSet::GetCriticalMultiplierAttribute(),
	  	EGameplayEffectAttributeCaptureSource::Source, true)
{
	RelevantAttributesToCapture.Append({
		AttackCapture, DefenseCapture, BonusCapture,
		IncomingCapture, ChanceCapture, CriticalCapture
	});
}

/**
 * 校验目标、读取聚合属性和技能输入、判定暴击、计算伤害，并提交正数中转值。
 * @param Params 本次 Spec、双方 ASC、标签与属性捕获的求值参数。
 * @param Output 输出集合，加入正 IncomingDamage 供属性集消费。
 */
void UCC_DamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
                                                 FGameplayEffectCustomExecutionOutput& Output) const
{
#if WITH_SERVER_CODE
	// 1. 结算只在目标权威端运行；生命和中转伤害都来自同一项目属性集。
	UAbilitySystemComponent* Target = Params.GetTargetAbilitySystemComponent();
	if (!Target || !Target->IsOwnerActorAuthoritative()
		|| !Target->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute())
		|| !Target->HasAttributeSetForAttribute(UCC_AttributeSet::GetIncomingDamageAttribute()))
		return;
	
	const float Health = Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute());
	if (!FMath::IsFinite(Health) || Health <= 0.f || Target->HasMatchingGameplayTag(
		StandaloneDamageTags::DamageImmunity))
		return;

	// 2. 每个目标/周期拥有独立元数据；复制 Context 不重新捕获攻击快照。
	if (FGameplayEffectSpec* MutableSpec = Params.GetOwningSpecForPreExecuteMod()) MutableSpec->
		DuplicateEffectContext();
	const FGameplayEffectSpec& Spec = Params.GetOwningSpec();
	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 3. GAS 已处理属性 Buff 聚合；不手动遍历活动 GE 再次叠加。
	float Attack = 0.f, Defense = 0.f, Bonus = 0.f, Incoming = 1.f, Chance = 0.f, Critical = 1.f;
	if (!Params.AttemptCalculateCapturedAttributeMagnitude(AttackCapture, Evaluation, Attack)
		|| !Params.AttemptCalculateCapturedAttributeMagnitude(DefenseCapture, Evaluation, Defense)
		|| !Params.AttemptCalculateCapturedAttributeMagnitude(BonusCapture, Evaluation, Bonus)
		|| !Params.AttemptCalculateCapturedAttributeMagnitude(IncomingCapture, Evaluation, Incoming)
		|| !Params.AttemptCalculateCapturedAttributeMagnitude(ChanceCapture, Evaluation, Chance)
		|| !Params.AttemptCalculateCapturedAttributeMagnitude(CriticalCapture, Evaluation, Critical))
		return;
	for (float Value : {Bonus, Incoming, Chance, Critical}) if (!FMath::IsFinite(Value))
		return;
	if (Incoming < 0.f || Critical < 1.f || Chance < 0.f || Chance > 1.f)
		return;

	// 4. 技能输入与角色属性在这里汇合；固定伤害仍受攻防规则影响。
	StandaloneDamage::FInput Input;
	Input.AttackPower = Attack;
	Input.Defense = Defense;
	Input.SkillScale = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, false, 1.f);
	Input.FlatDamage = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, false, 0.f);
	Input.DistanceMultiplier = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::DistanceMultiplier, false, 1.f);
	Input.HitMultiplier = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::HitMultiplier, false, 1.f);
	Input.OutgoingMultiplier = FMath::Max(0., 1. + static_cast<double>(Bonus));
	Input.IncomingMultiplier = Incoming;

	// 5. 服务器明确判定暴击；弱点倍率大于 1 不代表暴击。
	FGameplayEffectContextHandle Context = Spec.GetContext();
	if (auto* Typed = FCC_GameplayEffectContext::GetMutable(Context))
	{
		Typed->bCritical = Typed->CriticalPolicy == ECC_CriticalPolicy::Forced
			|| (Typed->CriticalPolicy == ECC_CriticalPolicy::Random && FMath::FRand() < Chance);
		if (Typed->bCritical) Input.HitMultiplier *= Critical;
	}

	// 6. 纯公式负责数值顺序、非法输入和溢出检查；这里不维护第二套公式。
	const StandaloneDamage::FResult Result = StandaloneDamage::Calculate(Input);
	if (!Result.IsValid() || Result.FinalDamage > MAX_flt)
	{
		UE_LOG(LogStandaloneDamage, Warning, TEXT("Invalid damage input or overflow; settlement skipped."));
		return;
	}
	if (Result.FinalDamage <= 0.) return;

	// 7. 一次执行只表示一击，不再乘 GE StackCount；完整伤害交给属性集消费和裁剪。
	Output.MarkStackCountHandledManually();
	Output.AddOutputModifier(FGameplayModifierEvaluatedData(UCC_AttributeSet::GetIncomingDamageAttribute(),
	                                                        EGameplayModOp::Additive,
	                                                        static_cast<float>(Result.FinalDamage)));
#endif
}
