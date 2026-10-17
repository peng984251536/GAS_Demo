#include "Damage/CC_DamageStatics.h"
#include "Damage/CC_DamageSettings.h"
#include "Damage/CC_DamageTags.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Effects/Damage/CC_GE_Damage.h"
#include "Effects/Damage/CC_GE_Healing.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"

namespace
{
	/** ASC 已初始化 Avatar，并且挂着项目属性集。 */
	bool IsReady(const UAbilitySystemComponent* ASC)
	{
		return IsValid(ASC) && IsValid(ASC->GetAvatarActor())
			&& ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute());
	}

	bool IsValidAmount(float Value) { return FMath::IsFinite(Value) && Value >= 0.f; }
}

ECC_DamageApplyStatus UCC_DamageStatics::ApplyDamage(UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC, const FCC_DamageRequest& Request)
{
	if (!IsReady(SourceASC) || !IsReady(TargetASC))
		return ECC_DamageApplyStatus::InvalidInput;
	if (!SourceASC->IsOwnerActorAuthoritative())
		return ECC_DamageApplyStatus::NotAuthority;
	
	const FGameplayEffectSpecHandle Spec = PrepareDamage(SourceASC, Request);
	return Spec.IsValid() ? ApplyPreparedDamage(TargetASC, Spec) : ECC_DamageApplyStatus::InvalidInput;
}

FGameplayEffectSpecHandle UCC_DamageStatics::PrepareDamage(UAbilitySystemComponent* SourceASC,
	const FCC_DamageRequest& Request, UObject* SourceObject)
{
	if (!IsReady(SourceASC) || !SourceASC->IsOwnerActorAuthoritative())
		return {};
	if (!IsValidAmount(Request.SkillScale) ||
		!IsValidAmount(Request.FlatDamage) ||
		!IsValidAmount(Request.DamageMultiplier))
		return {};

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	FCC_GameplayEffectContext* Typed = FCC_GameplayEffectContext::GetMutable(Context);
	if (!Typed)
		return {}; // DefaultGame.ini 未配置 CC_AbilitySystemGlobals。
	Typed->CriticalPolicy = Request.CriticalPolicy;
	if (Request.bHasHitResult)
		Context.AddHitResult(Request.HitResult);
	// 自定义的数据
	if (SourceObject)
		Context.AddSourceObject(SourceObject);

	// 创建 Spec 时捕获攻击方快照；目标属性在应用时读取。
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UCC_DamageSettings::GetDamageEffectClass(), 1.f, Context);
	
	if (!Spec.IsValid())
		return {};
	
	Spec.Data->SetSetByCallerMagnitude(CCDamageTags::SkillScale, Request.SkillScale);
	Spec.Data->SetSetByCallerMagnitude(CCDamageTags::FlatDamage, Request.FlatDamage);
	Spec.Data->SetSetByCallerMagnitude(CCDamageTags::DamageMultiplier, Request.DamageMultiplier);
	
	// 伤害类型和来源标签写进 Spec 标签：GE 修正器的标签条件、GameplayEvent 和 Cue 都能读到。
	Spec.Data->AppendDynamicAssetTags(Request.SourceTags);
	if (Request.DamageType.IsValid()) Spec.Data->AddDynamicAssetTag(Request.DamageType);
	return Spec;
}

ECC_DamageApplyStatus UCC_DamageStatics::ApplyPreparedDamage(UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpecHandle& Snapshot, const FHitResult* Hit)
{
	if (!IsReady(TargetASC) || !Snapshot.IsValid() || !Snapshot.Data->Def || !Snapshot.Data->Def->IsA<UCC_GE_Damage>())
		return ECC_DamageApplyStatus::InvalidInput;
	if (!TargetASC->IsOwnerActorAuthoritative()) return ECC_DamageApplyStatus::NotAuthority;

	FGameplayEffectSpec Spec(*Snapshot.Data);
	Spec.DuplicateEffectContext();
	if (Hit)
	{
		FGameplayEffectContextHandle Context = Spec.GetContext();
		Context.AddHitResult(*Hit, true);
	}
	return TargetASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied()
		? ECC_DamageApplyStatus::Applied : ECC_DamageApplyStatus::Rejected;
}

ECC_DamageApplyStatus UCC_DamageStatics::ApplyHealing(UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC, float Amount)
{
	if (!IsReady(SourceASC) || !IsReady(TargetASC) || !IsValidAmount(Amount)) return ECC_DamageApplyStatus::InvalidInput;
	if (!SourceASC->IsOwnerActorAuthoritative()) return ECC_DamageApplyStatus::NotAuthority;
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCC_DamageSettings::GetHealingEffectClass(), 1.f,
		SourceASC->MakeEffectContext());
	if (!Spec.IsValid()) return ECC_DamageApplyStatus::InvalidInput;
	Spec.Data->SetSetByCallerMagnitude(CCDamageTags::HealingAmount, Amount);
	return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied()
		? ECC_DamageApplyStatus::Applied : ECC_DamageApplyStatus::Rejected;
}
