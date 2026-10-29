// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_HitReact.generated.h"

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_HitReact : public UCC_GameplayAbility
{
	GENERATED_BODY()

public:
	UCC_HitReact();

	/**
	 * 激活前的门控：角色已死亡或正在销毁时不能受击。
	 * 霸体（Status.SuperArmor）与死亡标签由 ActivationBlockedTags 在这一步之前拦截。
	 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	static bool SetAvatarAILogicPaused(AActor* Avatar, bool bPaused);
	
	/**
	 * 获取受击的方向
	 * @param Instigator 
	 */
	UFUNCTION(BlueprintCallable, Category = "Crash|Abilities")
	void CacheHitDirectionVectors(const AActor* Instigator);

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	// 复用父类回调：Completed 正常结束，BlendOut 不提前结束，打断/取消也收尾。
	virtual void FinishOwnedAction(bool bWasCancelled) override;
	
	/**
	 * 当前自己的方向
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crash|Abilities")
	FVector AvatarForward;
	/**
	 * 敌人攻击的方向
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crash|Abilities")
	FVector ToInstigator;


};
