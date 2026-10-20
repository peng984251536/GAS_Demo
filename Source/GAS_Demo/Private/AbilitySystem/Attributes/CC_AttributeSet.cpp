#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Damage/CC_DamageTags.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
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
	, MoveSpeedMultiplier(1.f)
	, Defense(0.f)
	, OutgoingDamageBonus(0.f)
	, IncomingDamageMultiplier(1.f)
	, CriticalChance(0.f)
	, CriticalMultiplier(1.5f)
	, Damage(0.f)
	, Healing(0.f)
{
}

// 属性复制规则

/**
 * 登记持久属性的复制规则，中间属性只在服务器瞬时使用。
 * @param OutLifetimeProps 输出的属性复制规则列表。
 */
void UCC_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
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

namespace
{
	/** 把一次伤害结算以 GameplayEvent 发给受击者和来源，供 Buff / 技能（Ability Trigger）使用。 */
	void SendDamageEvents(const FGameplayEffectModCallbackData& Data, float HealthDamage)
	{
		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
		UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent();
		if (!SourceASC) SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Context.GetOriginalInstigator());
		FGameplayEventData Payload;
		Payload.Instigator = SourceASC ? SourceASC->GetAvatarActor() : Context.GetOriginalInstigator();
		Payload.Target = Data.Target.GetAvatarActor();
		Payload.EventMagnitude = HealthDamage;
		Payload.ContextHandle = Context; // 暴击、命中点
		Payload.InstigatorTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags(); // 含伤害类型和来源标签
		Payload.TargetTags = *Data.EffectSpec.CapturedTargetTags.GetAggregatedTags();

		Payload.EventTag = CCDamageTags::EventDamageTaken;
		Data.Target.HandleGameplayEvent(Payload.EventTag, &Payload);
		if (SourceASC)
		{
			Payload.EventTag = CCDamageTags::EventDamageDealt;
			SourceASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}
}

/**
 * 消费中间属性并修改生命。中间属性先读出再清零，事件回调里再次施加的效果不会读到残留值。
 * @param Data 本次 GE 的目标、Spec 和属性修改数据。
 */
void UCC_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float Amount = GetDamage();
		SetDamage(0.f);
		if (Amount > 0.f)
		{
			const float HealthBefore = GetHealth();
			SetHealth(FMath::Clamp(HealthBefore - Amount, 0.f, GetMaxHealth()));
			// 事件在扣血之后发送；EventMagnitude 是实际扣血量（残血时小于理论伤害）。
			SendDamageEvents(Data, HealthBefore - GetHealth());
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float Amount = GetHealing();
		SetHealing(0.f);
		if (Amount > 0.f) SetHealth(FMath::Clamp(GetHealth() + Amount, 0.f, GetMaxHealth()));
	}
	// 其他属性的范围由 ClampAttribute（Pre*Change）和 PostAttributeChange 统一维护，这里不再重复裁剪。
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

void UCC_AttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MoveSpeedMultiplier, OldValue);
}
void UCC_AttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
    if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue) SetHealth(FMath::Max(0.f, NewValue));
    if (Attribute == GetMaxManaAttribute() && GetMana() > NewValue) SetMana(FMath::Max(0.f, NewValue));
}
