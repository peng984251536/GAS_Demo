// Fill out your copyright notice in the Description page of Project Settings.


#include "Attribute/CC_AttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UCC_AttributeSet::UCC_AttributeSet()
// 这是 C++ 层的兜底初始值；正式项目建议用一个 Instant GameplayEffect Override
// 来统一配置玩家、敌人和装备带来的初始属性。
	: Health(100.f)
	  , MaxHealth(100.f)
	  , Mana(50.f)
	  , MaxMana(50.f)
	  , AttackPower(10.f)
{

	
}

void UCC_AttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, AttackPower, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(
	ThisClass, bAttributesInitialized, COND_None, REPNOTIFY_Always);
	//DOREPLIFETIME(ThisClass,bAttributesInitialized);
	
}

/**
 * 数值变化之前
 * @param Attribute 
 * @param NewValue 
 */
void UCC_AttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

/**
 * 保证数值处于合理范围
 * @param Data 
 */
void UCC_AttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 处理生命值上限变化之后
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		const float ClampedHealth =
			FMath::Clamp(GetHealth(), 0.f, GetMaxHealth());

		if (!FMath::IsNearlyEqual(GetHealth(), ClampedHealth))
		{
			SetHealth(ClampedHealth);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetMaxManaAttribute())
	{
		const float ClampedMana =
			FMath::Clamp(GetMana(), 0.f, GetMaxMana());

		if (!FMath::IsNearlyEqual(GetMana(), ClampedMana))
		{
			SetMana(ClampedMana);
		}
	}


	if(!bAttributesInitialized)
	{
		bAttributesInitialized = true;
		OnAttributesInitialized.Broadcast();
	}
}


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


void UCC_AttributeSet::OnRep_AttributesInitialized()
{
	if(bAttributesInitialized)
	{
		OnAttributesInitialized.Broadcast();
	}
}