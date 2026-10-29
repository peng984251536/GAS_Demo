#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
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
 * 然后重新 MoveTo，不会出现两条移动任务互相取消。设计见 Docs/AI/AI-Combat.zh-CN.md。
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

	/**
	 * 近战追击时比 MeleeReach 多走近的距离（厘米）：停止距离 = MeleeReach − 它（至少 10）。
	 * 引擎的到达判定会把自身半径略微放大，正好停在 MeleeReach 边界时，行为树的攻击距离判断
	 * （CC In Attack Range，按 MeleeReach）可能差一两厘米判为「够不着」，导致追到了却一直不出刀。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0"))
	float MeleeApproachMargin = 20.f;

	/**
	 * 一次移动最长持续时间（秒），超过后结束能力，由行为树重新判断再发起。
	 * 与看门狗一起保证移动被打断（受击、新的移动请求）时能力一定会结束，行为树不会卡在这个节点。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.5"))
	float MaxMoveDuration = 5.f;

private:

	/**
	 * 弓兵走位：在目标当前位置周围找一个射程内（MinRange～MaxRange）可达的站位走过去。
	 * 候选为 2 个距离（射程中线、中线与最小射程之间）× 8 个方向，从自己相对目标的方向开始左右交替。
	 * 一个都找不到（场地狭小、贴墙）时直接朝目标走到射程中线以内，不会原地放弃。
	 */
	void StartRangedApproach(const ACC_BaseCharacter& Enemy, AActor& Target);

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

	/** 看门狗：移动已被接管或超时则结束能力（见 CCMoveTaskWatchdog）。 */
	void CheckMoveWatchdog();

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;

	FTimerHandle MoveWatchdogTimer;
	TWeakObjectPtr<AAIController> WatchedController;
	uint32 WatchedRequestId = 0;
	float MoveStartTime = 0.f;
};
