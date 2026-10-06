#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "CC_FollowTarget.generated.h"

class UAITask_MoveTo;
class AAIController;

/** 激活后跟随当前目标。基础结构已就绪，具体跟随逻辑在 ActivateAbility 中补全。 */
UCLASS()
class GAS_DEMO_API UCC_FollowTarget : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_FollowTarget();

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/** 创建蓝图 Move To Location or Actor 对应的异步任务并绑定完成回调。 */
	void StartMoveToTarget(const ACC_BaseCharacter& Enemy, const FVector& knownLocation);
	/**
	 * 结束移动
	 * @param Result 
	 * @param AIController 
	 */
	void HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* AIController);

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;

	
};
