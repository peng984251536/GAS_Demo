#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_Death.generated.h"

/**
 * 角色死亡：服务器结算死亡并播放死亡蒙太奇；拥有者客户端只播放蒙太奇（ServerInitiated）。
 * 死亡生命周期（DeathFinished 上报、复活取消、怪物销毁）只由服务器决定，客户端结束不会同步给服务器。
 */
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
	// 拥有者客户端只在本地结束（不复制结束），服务器路径负责 DeathFinished 与怪物销毁。
	virtual void FinishOwnedAction(bool bWasCancelled) override;
};
