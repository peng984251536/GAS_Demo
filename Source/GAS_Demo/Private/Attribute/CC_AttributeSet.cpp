#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Damage/CC_DamageTypes.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

/**
 * 设置资源和战斗属性的兜底初值，角色初始化 GE 可覆盖基础值。
 */
UCC_AttributeSet::UCC_AttributeSet()
	: Health(100.f)
	, MaxHealth(100.f)
	, Mana(50.f)
	, MaxMana(50.f)
	, AttackPower(10.f)
	, Defense(0.f)
	, OutgoingDamageBonus(0.f)
	, IncomingDamageMultiplier(1.f)
	, CriticalChance(0.f)
	, CriticalMultiplier(1.5f)
	, IncomingDamage(0.f)
	, IncomingHealing(0.f)
{
}

// 属性复制规则

/**
 * 登记持久属性的复制规则，中转数值只在服务器瞬时使用。
 * @param OutLifetimeProps 输出的属性复制规则列表。
 */
void UCC_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, OutgoingDamageBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, CriticalChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, CriticalMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, bAttributesInitialized, COND_None, REPNOTIFY_Always);
}

// 属性变化与 GE 结算

/**
 * 基础属性值变化前限制合法范围。
 * @param Attribute 正在变化的属性字段。
 * @param Value 待写入的基础值，按引用裁剪。
 */
void UCC_AttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& Value) const
{
	Super::PreAttributeBaseChange(Attribute, Value);
	ClampAttribute(Attribute, Value);
}

/**
 * 当前属性值变化前限制合法范围。
 * @param Attribute 正在变化的属性字段。
 * @param Value 待写入的当前值，按引用裁剪。
 */
void UCC_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value);
	ClampAttribute(Attribute, Value);
}

/**
 * 执行前检查权威、合法中转量和免疫状态。
 * @param Data 本次 GE 的目标、Spec 和属性修改数据。
 * @return 允许执行返回 true，否则返回 false。
 */
bool UCC_AttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data)) return false;
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute() || Data.EvaluatedData.Attribute ==
		GetIncomingHealingAttribute())
	{
		if (!Data.Target.IsOwnerActorAuthoritative() || !FMath::IsFinite(Data.EvaluatedData.Magnitude)
			|| Data.EvaluatedData.Magnitude < 0.f)
			return false;
		if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute()
			&& (GetHealth() <= 0.f || Data.Target.HasMatchingGameplayTag(StandaloneDamageTags::DamageImmunity)))
			return false;
	}
	return true;
}

/**
 * 消费中转量、修改实际生命并发布结算结果，保持回调重入时的结果独立。
 * @param Data 本次 GE 的目标、Spec 和属性修改数据。
 */
void UCC_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	const bool bDamage = Data.EvaluatedData.Attribute == GetIncomingDamageAttribute();
	const bool bHealing = Data.EvaluatedData.Attribute == GetIncomingHealingAttribute();
	if (bDamage || bHealing)
	{
		// 先保存到局部变量并清零中转属性，防止同步回调中新效果复用上次残留值。
		const float Amount = bDamage ? GetIncomingDamage() : GetIncomingHealing();
		if (bDamage) SetIncomingDamage(0.f);
		else SetIncomingHealing(0.f);
		if (Data.Target.IsOwnerActorAuthoritative() && FMath::IsFinite(Amount) && Amount > 0.f)
		{
			// 结果采用值副本；残血时 ResolvedDamage 可以大于实际 HealthDamage。
			FCC_DamageResolved Result;
			const auto& Context = Data.EffectSpec.GetContext();
			Result.Source = Context.GetOriginalInstigatorAbilitySystemComponent()
				                ? Context.GetOriginalInstigatorAbilitySystemComponent()->GetAvatarActor()
				                : Context.GetOriginalInstigator();
			Result.Target = Data.Target.GetAvatarActor();
			Result.HealthBefore = GetHealth();
			Result.HealthAfter = FMath::Clamp(Result.HealthBefore + (bHealing ? Amount : -Amount), 0.f, GetMaxHealth());
			Result.ResolvedDamage = Amount;
			Result.HealthDamage = FMath::Abs(Result.HealthBefore - Result.HealthAfter);
			Result.bHealing = bHealing;
			Result.bKilled = bDamage && Result.HealthBefore > 0.f && Result.HealthAfter <= 0.f;
			if (const auto* Typed = FCC_GameplayEffectContext::Get(Context))
			{
				Result.DamageType = Typed->DamageType;
				Result.bCritical = !bHealing && Typed->bCritical;
				if (Typed->bUseHitLocation && Context.GetHitResult()) Result.TextLocation = Context.GetHitResult()->
					ImpactPoint;
				else if (Result.Target)
				{
					FVector Extent;
					Result.Target->GetActorBounds(true, Result.TextLocation, Extent);
					Result.TextLocation.Z += Extent.Z + 20.f;
				}
			}
			else if (Result.Target)
			{
				FVector Extent;
				Result.Target->GetActorBounds(true, Result.TextLocation, Extent);
				Result.TextLocation.Z += Extent.Z + 20.f;
			}
			// SetHealth 会同步触发生命委托，甚至再次施加效果；必须提前确定本击结果，避免重入覆盖。
			SetHealth(Result.HealthAfter);
			// 普通 ASC 可订阅属性集委托；项目 ASC 继续通知自动创建的反馈组件。
			OnDamageResolved.Broadcast(Result);
			if (auto* ASC = Cast<UCC_AbilitySystemComponent>(&Data.Target)) ASC->PublishDamageResult(Result);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute()) SetHealth(
		FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	else if (Data.EvaluatedData.Attribute == GetMaxManaAttribute()) SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute()) SetHealth(
		FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	if (!bAttributesInitialized)
	{
		bAttributesInitialized = true;
		OnAttributesInitialized.Broadcast();
	}
}

// 属性复制回调：沿用原来的明确函数写法

void UCC_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Health, OldValue);
}

void UCC_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxHealth, OldValue);
}

void UCC_AttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Mana, OldValue);
}

void UCC_AttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxMana, OldValue);
}

void UCC_AttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, AttackPower, OldValue);
}

void UCC_AttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Defense, OldValue);
}

void UCC_AttributeSet::OnRep_OutgoingDamageBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, OutgoingDamageBonus, OldValue);
}

void UCC_AttributeSet::OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, IncomingDamageMultiplier, OldValue);
}

void UCC_AttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, CriticalChance, OldValue);
}

void UCC_AttributeSet::OnRep_CriticalMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, CriticalMultiplier, OldValue);
}

void UCC_AttributeSet::OnRep_AttributesInitialized()
{
	if (bAttributesInitialized) OnAttributesInitialized.Broadcast();
}

// 内部数值边界

/**
 * 统一限制资源、倍率和暴击属性，不重新计算 Buff 或伤害。
 * @param Attribute 待限制的属性字段。
 * @param Value 待写入的值，按引用原地裁剪。
 */
void UCC_AttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const
{
	if (!FMath::IsFinite(Value)) Value = 0.f;
	if (Attribute == GetHealthAttribute()) Value = FMath::Clamp(Value, 0.f, GetMaxHealth());
	else if (Attribute == GetManaAttribute()) Value = FMath::Clamp(Value, 0.f, GetMaxMana());
	else if (Attribute == GetCriticalChanceAttribute()) Value = FMath::Clamp(Value, 0.f, 1.f);
	else if (Attribute == GetCriticalMultiplierAttribute()) Value = FMath::Max(1.f, Value);
	else if (Attribute == GetOutgoingDamageBonusAttribute()) Value = FMath::Max(-1.f, Value);
	else Value = FMath::Max(0.f, Value);
}
