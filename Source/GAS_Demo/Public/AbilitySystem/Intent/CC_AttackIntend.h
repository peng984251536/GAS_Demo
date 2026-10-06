// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_AttackIntend.generated.h"

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_AttackIntend : public UCC_GameplayAbility
{
	GENERATED_BODY()

protected:
	/** 激活前门控：镜像 CanStartAction 的优先级/连段窗口判定，未满足则不激活。 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	/** 激活：数据驱动选招（或读显式 payload）→ ASC 门控+提交 → 经注册表派发对应机制 GA。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
};
