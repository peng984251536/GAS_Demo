#pragma once

#include "CoreMinimal.h"

/**
 * 环上的一个槽位。纯数据，不引用任何 Actor。
 */
struct GAS_DEMO_API FCC_Slot
{
	/** 相对环中心的世界方向偏移；只随中心平移，不随目标朝向旋转。 */
	FVector LocalOffset = FVector::ZeroVector;
	/** 当前世界位置；导航校验成功后为投影到 NavMesh 上的点。 */
	FVector WorldLocation = FVector::ZeroVector;
	/** 是否可用：投影成功、未被墙挡住、不在冷却中。由调用方校验后写入。 */
	bool bUsable = false;
	/** 本次分配中是否已被占用。 */
	bool bOccupied = false;
	/** 冷却结束的世界时间（秒）；冷却中的槽位不可用。 */
	float CooldownUntil = 0.f;
};

/**
 * 围绕一个中心的环形槽位：生成期望位置，并做增量分配。
 * 纯逻辑，不依赖 Actor、ASC 或导航系统；导航校验由 UCC_EngagementSubsystem 负责。
 */
struct GAS_DEMO_API FCC_SlotRing
{
	float Radius = 0.f;
	float Spacing = 0.f;
	TArray<FCC_Slot> Slots;

	/**
	 * 计算给定半径与间距下的槽位数。
	 * @param InRadius 环半径（厘米）。
	 * @param InSpacing 槽位间距（厘米）。
	 * @return 至少为 1 的槽位数。
	 */
	static int32 ComputeSlotCount(float InRadius, float InSpacing);

	/**
	 * 把环移动到新中心。半径或间距变化（或第一次调用）时重新生成槽位，此时旧槽位索引失效。
	 * 所有槽位的 bUsable 重置为 true（冷却中的除外），等待调用方做导航校验；占用标记与冷却保留。
	 * @param Center 新中心。
	 * @param InRadius 环半径。
	 * @param InSpacing 槽位间距。
	 * @param Jitter 固定随机偏移幅度。
	 * @param Now 当前世界时间，用于判断冷却。
	 * @return 重新生成了槽位时返回 true。
	 */
	bool Rebuild(const FVector& Center, float InRadius, float InSpacing, float Jitter, float Now);

	/** 清空所有占用标记。 */
	void ClearOccupancy();

	/** @return 索引有效、可用且未被占用。 */
	bool IsFree(int32 Index) const;

	/** 标记占用；索引无效时忽略。 */
	void Occupy(int32 Index);

	/** 取消占用；索引无效时忽略。 */
	void Vacate(int32 Index);

	/** @return 可用且未被占用的槽位数。 */
	int32 CountFreeUsable() const;

	/**
	 * 找离给定位置最近的空闲可用槽位。
	 * @param Position 参考位置。
	 * @return 槽位索引；没有时返回 INDEX_NONE。
	 */
	int32 FindFreeSlotNear(const FVector& Position) const;

	/**
	 * 增量分配。先清空占用，再执行：
	 *   1. 保留：旧槽位仍然空闲可用，且攻击者相对中心的方位与槽位方位夹角不超过 90° 的，原样保留；
	 *   2. 补位：其余攻击者按离中心由近到远，各自挑离自己最近的空闲可用槽位；
	 *   3. 没有槽位的写 INDEX_NONE，由调用方降档。
	 * @param Center 环中心。
	 * @param Positions 攻击者位置，与 InOutSlotIndices 一一对应。
	 * @param InOutSlotIndices 输入上次的槽位索引，输出新的槽位索引。
	 */
	void AssignIncremental(const FVector& Center, TConstArrayView<FVector> Positions, TArray<int32>& InOutSlotIndices);
};
