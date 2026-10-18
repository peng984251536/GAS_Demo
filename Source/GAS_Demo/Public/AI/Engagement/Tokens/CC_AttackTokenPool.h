#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/ObjectKey.h"

/**
 * 攻击令牌池。只认 FObjectKey 这个标识，不关心它是什么对象。
 * 纯逻辑，不依赖 Actor、ASC 或槽位；「发令牌必须同时占内圈槽位」由 UCC_EngagementSubsystem 保证。
 */
struct GAS_DEMO_API FCC_AttackTokenPool
{
	/** 配置容量。 */
	int32 Capacity = 4;

	/**
	 * 申请令牌。已持有时直接返回 true（幂等），不重复占用。
	 * @param Key 申请者标识。
	 * @param Cost 消耗的令牌数，小于 1 时按 1 处理。
	 * @return 持有令牌时返回 true；余量不足返回 false。
	 */
	bool TryAcquire(const FObjectKey& Key, int32 Cost = 1);

	/**
	 * 归还令牌。
	 * @return 原本持有并已归还时返回 true。
	 */
	bool Release(const FObjectKey& Key);

	bool Holds(const FObjectKey& Key) const;

	/** @return 已占用的令牌数（按消耗值累计）。 */
	int32 Used() const;

	/** @return 剩余可发放的令牌数。 */
	int32 Available() const;

	/** 移除满足条件的持有者，用于清理已失效的攻击者。 */
	void PruneIf(TFunctionRef<bool(const FObjectKey&)> ShouldRemove);

	void Reset();

private:
	TMap<FObjectKey, int32> Holders;
};
