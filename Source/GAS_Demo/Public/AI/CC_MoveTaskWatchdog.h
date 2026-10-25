#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

/**
 * 移动类能力（CC_FollowTarget / CC_KeepDistance）的看门狗工具。
 *
 * 这些能力发起 UAITask_MoveTo 后等待 OnMoveTaskFinished 才结束。但移动被「新的移动请求」顶掉时，
 * UAITask_MoveTo 会故意忽略这次结束；任务因资源被抢而挂起时也不会广播。能力于是永远不结束，
 * 行为树等待能力的任务节点（BTT_ActiveAbility）随之卡死。看门狗定期检查并在这些情况下结束能力。
 */
namespace CCMoveTaskWatchdog
{
	/** 看门狗检查间隔（秒）。 */
	constexpr float CheckInterval = 0.25f;

	/** @return 寻路组件当前请求的 ID；没有寻路组件时为 0。在 ReadyForActivation 之后立即记录。 */
	inline uint32 GetCurrentRequestId(const AAIController* Controller)
	{
		const UPathFollowingComponent* PathFollowing = IsValid(Controller) ? Controller->GetPathFollowingComponent() : nullptr;
		return PathFollowing ? PathFollowing->GetCurrentRequestId().GetID() : 0u;
	}

	/**
	 * @param Controller 执行移动的控制器。
	 * @param WatchedRequestId 本次移动发起后记录的请求 ID。
	 * @return 本次移动已经不在执行：寻路组件空闲，或正在执行的已是别的请求。
	 */
	inline bool IsMoveOrphaned(const AAIController* Controller, uint32 WatchedRequestId)
	{
		const UPathFollowingComponent* PathFollowing = IsValid(Controller) ? Controller->GetPathFollowingComponent() : nullptr;
		if (!PathFollowing)
		{
			return true;
		}
		return PathFollowing->GetStatus() == EPathFollowingStatus::Idle
			|| PathFollowing->GetCurrentRequestId().GetID() != WatchedRequestId;
	}
}
