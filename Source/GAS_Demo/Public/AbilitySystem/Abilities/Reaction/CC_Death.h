#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_Death.generated.h"

/** 怪物死亡：播放死亡蒙太奇，结束后由服务器销毁怪物。 */
UCLASS()
class GAS_DEMO_API UCC_Death : public UCC_GameplayAbility
{
	GENERATED_BODY()

public:
	UCC_Death();

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle,
									  const FGameplayAbilityActorInfo* ActorInfo,
									  const FGameplayAbilityActivationInfo ActivationInfo,
									  FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
									  const FGameplayEventData* TriggerEventData);
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 复用父类回调：Completed 正常结束，BlendOut 不提前结束，打断/取消也收尾。
	virtual void FinishOwnedAction(bool bWasCancelled) override;
};
