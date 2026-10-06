#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "Navigation/PathFollowingComponent.h"
#include "CC_FindPlayerTarget.generated.h"

class AAIController;
class ACC_EnemyCharacter;
class UAITask_MoveTo;

/** 激活后查找玩家并异步移动到其附近，移动完成或失败时结束能力。 */
UCLASS()
class GAS_DEMO_API UCC_FindPlayerTarget : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_FindPlayerTarget();

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 创建蓝图 Move To Location or Actor 对应的异步任务并绑定完成回调。 */
	void StartMoveToTarget(ACC_BaseCharacter& Enemy, AActor& TargetActor);
	/**
	 * 结束移动
	 * @param Result 
	 * @param AIController 
	 */
	void HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* AIController);

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;
};
