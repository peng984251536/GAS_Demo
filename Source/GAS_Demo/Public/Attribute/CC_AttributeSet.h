// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CC_AttributeSet.generated.h"

#define  ATTRIBUTE_ACCESSORS(ClassName,PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName,PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributesInitialized);

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCC_AttributeSet();

	//但 bAttributesInitialized 在头文件里是 ReplicatedUsing=OnRep_AttributesInitialized，
	//而其他属性全是 REPNOTIFY_Always。
	//崩溃正是 UE 检测到同一个复制编号出现了不同的 RepNotifyCondition。
	UPROPERTY(ReplicatedUsing=OnRep_AttributesInitialized)
	bool bAttributesInitialized = false;
	UFUNCTION()
	void OnRep_AttributesInitialized();

	UPROPERTY(BlueprintAssignable)
	FAttributesInitialized OnAttributesInitialized;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 属性改变前
	 * @param Attribute 
	 * @param NewValue 
	 */
	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute,
		float& NewValue) override;

	/**
	 * 属性改变后
	 * @param Data 
	 */
	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data) override;
	
	/** 当前生命值。伤害和治疗都应该通过 GameplayEffect 修改它。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes|Vital")
	FGameplayAttributeData Health;
	/** 最大生命值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	/** 当前法力值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Mana, Category="Attributes|Vital")
	FGameplayAttributeData Mana;
	/** 最大法力值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMana, Category="Attributes|Vital")
	FGameplayAttributeData MaxMana;
	/** 基础攻击力，最终伤害可在 GameplayEffect 中结合技能倍率计算。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackPower, Category="Attributes|Vital")
	FGameplayAttributeData AttackPower;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	ATTRIBUTE_ACCESSORS(ThisClass, Health);
	ATTRIBUTE_ACCESSORS(ThisClass, MaxHealth);
	ATTRIBUTE_ACCESSORS(ThisClass, Mana);
	ATTRIBUTE_ACCESSORS(ThisClass, MaxMana);
	ATTRIBUTE_ACCESSORS(ThisClass, AttackPower);
};
