#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "CC_KeepDistance.generated.h"

class AAIController;
class UAITask_MoveTo;

/** 目标靠近后执行一次撤退；下次是否还需要撤退由 AI 周期检查决定。 */
UCLASS()
class GAS_DEMO_API UCC_KeepDistance : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_KeepDistance();

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
	void HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* Controller);
	void HandlePathRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void LogMoveProgress();
	void StopMoveDiagnostics();

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;

	/** 仅在本次撤退期间监听寻路结果，以看到 Aborted 的具体标志。 */
	TWeakObjectPtr<UPathFollowingComponent> ObservedPathFollowing;
	FDelegateHandle PathRequestFinishedHandle;
	FTimerHandle MoveProgressTimer;
	FVector MoveDestination = FVector::ZeroVector;
};
