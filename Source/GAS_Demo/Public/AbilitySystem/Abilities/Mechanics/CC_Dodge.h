// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_Dodge.generated.h"

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_Dodge : public UCC_GameplayAbility
{
	GENERATED_BODY()

public:
	/** 激活：数据驱动选招（或读显式 payload）→ ASC 门控+提交 → 经注册表派发对应机制 GA。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;



	
};
