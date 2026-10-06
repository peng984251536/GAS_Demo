#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "CC_BowShoot.generated.h"

class UAbilityTask_WaitGameplayEvent;

/** 独立的弓箭攻击：播放动作、在放箭时刻生成箭，命中由箭 Actor 自己处理。 */
UCLASS()
class GAS_DEMO_API UCC_BowShoot : public UCC_GameplayAbility
{
	GENERATED_BODY()

public:
	UCC_BowShoot();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 动画 Notify 发来放箭事件时调用；必须是 UFUNCTION 才能绑定动态委托。 */
	UFUNCTION()
	void OnBowRelease(FGameplayEventData Payload);
	void ReleaseArrow();

	
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ReleaseEventTask;
	bool bArrowReleased = false;
};
