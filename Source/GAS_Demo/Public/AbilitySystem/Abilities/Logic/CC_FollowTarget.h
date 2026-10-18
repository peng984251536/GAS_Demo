#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "CC_FollowTarget.generated.h"

class UAITask_MoveTo;
class AAIController;

/**
 * 走到分配位置（所有档位通用）。
 *
 * 优先读取 UCC_EngagementSubsystem 给出的 MoveLocation（内圈 / 外圈 / 远程圈槽位或排队点）；
 * 没有有效分配时回退到旧逻辑：围绕 LastKnownLocation 按角度候选选点。
 *
 * 允许重新触发：分配位置变化后行为树再次发送 FollowTarget 事件，旧的移动任务会在 EndAbility 中
 * 先解绑再结束，然后按新位置重新 MoveTo，不会出现两条移动任务互相取消。
 */
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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 走向内圈槽位（持有令牌）时的可接受半径。 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Engagement", meta = (ClampMin = "1"))
	float EngagedAcceptanceRadius = 30.f;

	/** 走向外圈 / 远程圈槽位或排队点时的可接受半径。 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Engagement", meta = (ClampMin = "1"))
	float DefaultAcceptanceRadius = 50.f;

private:
	/**
	 * 从围攻子系统读取分配位置。
	 * @param OutDestination 分配位置。
	 * @param OutAcceptanceRadius 按档位选择的可接受半径。
	 * @return 有有效分配时返回 true。
	 */
	bool TryGetEngagementDestination(FVector& OutDestination, float& OutAcceptanceRadius) const;

	/** 回退逻辑：围绕已知位置按角度候选选出可达点，然后移动过去。 */
	void StartMoveToTarget(const ACC_BaseCharacter& Enemy, const FVector& knownLocation);

	/**
	 * 创建 UAITask_MoveTo 并绑定完成回调。
	 * @param AIController 执行移动的控制器。
	 * @param Destination 目标点。
	 * @param AcceptanceRadius 可接受半径。
	 */
	void StartMoveTask(AAIController* AIController, const FVector& Destination, float AcceptanceRadius);

	/**
	 * 结束移动
	 * @param Result
	 * @param AIController
	 */
	void HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* AIController);

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;
};
