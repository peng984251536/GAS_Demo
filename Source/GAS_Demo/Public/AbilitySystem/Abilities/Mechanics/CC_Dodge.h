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

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	/** 闪避期间把 CharacterMovement 的转向速度清零，避免边闪避边转身；结束时恢复。 */
	void LockRotation(const FGameplayAbilityActorInfo* ActorInfo);
	void RestoreRotation(const FGameplayAbilityActorInfo* ActorInfo);

	/** 激活前的转向速度（运行时状态，每次激活记录一次）。 */
	FRotator SavedRotationRate = FRotator::ZeroRotator;
	bool bRotationLocked = false;
};
