#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "CC_FollowTarget.generated.h"

class UAITask_MoveTo;
class AAIController;

/**
 * 走向当前目标。
 *
 * - 近战：以目标 Actor 为终点 MoveTo，可接受半径 = 角色配置的 MeleeReach，到达判定包含双方碰撞半径
 *   （即边缘距离）。目标移动时引擎自动更新路径，进入攻击距离后能力结束，行为树接着攻击。
 *   不分配站位：同伴之间由 Detour Crowd 避让，正在攻击的同伴是障碍（ObstacleOnly），包围圈自然形成。
 * - 弓兵，或近战已没有存活目标（只剩 LastKnownLocation）：围绕已知位置按角度候选选出射程中线上的可达点。
 *
 * 允许重新触发：行为树再次发送 FollowTarget 事件时，旧的移动任务会在 EndAbility 中先解绑再结束，
 * 然后重新 MoveTo，不会出现两条移动任务互相取消。设计见 Docs/AI-Combat.zh-CN.md。
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

	/** 弓兵走向射程中线选点时的可接受半径。近战的可接受半径来自角色配置的 MeleeReach。 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "1"))
	float DefaultAcceptanceRadius = 50.f;

private:

	/** 回退逻辑：围绕已知位置按角度候选选出可达点，然后移动过去。 */
	void StartMoveToTarget(const ACC_BaseCharacter& Enemy, const FVector& knownLocation);

	/**
	 * 创建 UAITask_MoveTo 并绑定完成回调。
	 * @param AIController 执行移动的控制器。
	 * @param Destination 目标点；GoalActor 不为空时忽略。
	 * @param AcceptanceRadius 可接受半径。
	 * @param GoalActor 以 Actor 为终点（跟随其移动，到达判定包含双方碰撞半径）；为空时走向 Destination。
	 */
	void StartMoveTask(AAIController* AIController, const FVector& Destination, float AcceptanceRadius,
		AActor* GoalActor = nullptr);

	/**
	 * 结束移动
	 * @param Result
	 * @param AIController
	 */
	void HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* AIController);

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;
};
