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
	UCC_Dodge();

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
	/**
	 * 闪避期间把 CharacterMovement 的转向速度清零，避免边闪避边转身；结束时恢复。
	 * 用 Status.RotationLocked 松散标签计数：多个闪避实例重叠时，最后一个结束才恢复，
	 * 且恢复为移动组件的默认配置值（Archetype），不依赖"激活时保存的值"，因此不会卡在 0。
	 */
	void LockRotation(const FGameplayAbilityActorInfo* ActorInfo);
	void RestoreRotation(const FGameplayAbilityActorInfo* ActorInfo);

	/** 本实例是否持有一份转向锁。 */
	bool bRotationLocked = false;
};
