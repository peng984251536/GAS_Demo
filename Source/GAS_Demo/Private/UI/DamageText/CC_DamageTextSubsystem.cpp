// 飘字管理流程：ReportHit 提交或合并命中 → Tick 老化数据 → HUD 的 OnPaint 读取活动列表。
#include "UI/DamageText/CC_DamageTextSubsystem.h"

#include "Engine/World.h"

// 排除专用服务器；单机、客户端与监听服务器的本机表现仍可使用此子系统。
bool UCC_DamageTextSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// 只有客户端需要表现层。专用服务器上不创建，省掉一份永不使用的活动列表。
	// 数值本身由 GameplayCue 复制到客户端，服务器不需要在这里留存任何数据。
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->GetNetMode() != NM_DedicatedServer;
	}
	return true;
}

// 将集中数据管理的 Tick 时间纳入引擎性能统计。
TStatId UCC_DamageTextSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCC_DamageTextSubsystem, STATGROUP_Tickables);
}

// 由基类先确认对象可运行，再检查活动数据；没有条目时不进入 Tick 处理。
bool UCC_DamageTextSubsystem::IsTickable() const
{
	// 必须与基类结果取与：基类负责排除模板对象和未初始化状态，
	// 直接返回 ActiveEntries.Num() > 0 会让 CDO 也被纳入 Tick 判定。
	//
	// 没有活动飘字时不 Tick，这是空闲期零开销的关键：
	// 战斗结束后子系统完全不参与帧循环，而不是每帧空转一遍。
	return Super::IsTickable() && ActiveEntries.Num() > 0;
}

// 命中入口：零值忽略，先尝试合并，未合并才新建结构体条目，不创建 UI 对象。
void UCC_DamageTextSubsystem::ReportHit(const FVector& WorldLocation, float Amount,
	ECC_DamageTextStyle Style)
{
	// 数值为 0 的命中不值得飘字（免伤、治疗溢出、被规则拒绝）。
	if (FMath::IsNearlyZero(Amount))
	{
		return;
	}

	LastMergeCount = 0;

	if (TryMerge(WorldLocation, Amount, Style))
	{
		return;
	}

	FCC_DamageTextEntry Entry;
	Entry.WorldLocation = WorldLocation;
	Entry.Amount = Amount;
	Entry.Style = Style;
	Entry.Elapsed = 0.0f;
	Entry.StackOffset = NextStackOffset();
	Entry.Pulse = 0.0f;

	// 新飘字插到最前，绘制顺序上压住旧飘字，视觉上更符合"最新命中在上面"。
	ActiveEntries.Insert(Entry, 0);
}

// 按锚点距离和距最后一次命中的时间匹配；不保存目标身份，也没有区分伤害与治疗的独立合并组。
bool UCC_DamageTextSubsystem::TryMerge(const FVector& WorldLocation, float Amount,
	ECC_DamageTextStyle Style)
{
	if (MergeWindow <= 0.0f)
	{
		return false;
	}

	// 用世界空间距离判定"是不是同一个目标"。
	//
	// 为什么不按屏幕距离：屏幕距离需要给每个条目做一次投影，是每帧每条的额外开销；
	// 而这里只在命中发生时才比较一次，且目的是区分不同敌人（通常相距几百厘米），
	// 世界距离已经足够区分。真正的屏幕空间错开交给 StackOffset 处理。
	const float ToleranceSq = MergeDistance * MergeDistance;

	for (FCC_DamageTextEntry& Entry : ActiveEntries)
	{
		// 只有还在合并窗口内的才允许并入，否则连击会无限累加成一个永不消失的数字。
		if (Entry.Elapsed > MergeWindow)
		{
			continue;
		}

		if (FVector::DistSquared(Entry.WorldLocation, WorldLocation) > ToleranceSq)
		{
			continue;
		}

		Entry.Amount += Amount;
		Entry.Elapsed = 0.0f;   // 刷新计时，让数字继续存在
		if (Entry.Pulse < 1.0f)
		{
			Entry.Pulse = 1.0f;  // 触发一次弹跳
		}

		// 样式取更"重要"的那个：暴击压过治疗，治疗压过普通。
		// 这样连击里出现一次暴击，整个数字会升格成暴击表现，符合玩家直觉。
		if (Style == ECC_DamageTextStyle::Critical ||
			(Style == ECC_DamageTextStyle::Heal && Entry.Style == ECC_DamageTextStyle::Normal))
		{
			Entry.Style = Style;
		}

		LastMergeCount = 1;
		return true;
	}

	return false;
}

// 推进计时并衰减合并脉冲，超时后移除；RemoveAtSwap 不保留原有条目顺序。
void UCC_DamageTextSubsystem::AgeEntries(float DeltaTime)
{
	// 倒序遍历，边遍历边删。活动数量很小（通常十几个），不做对象池——
	// 每次命中新建一个 struct 的成本远低于维护池的复杂度。
	for (int32 Index = ActiveEntries.Num() - 1; Index >= 0; --Index)
	{
		FCC_DamageTextEntry& Entry = ActiveEntries[Index];
		Entry.Elapsed += DeltaTime;

		// 弹跳衰减：命中后快速回落到 0，用来做一次性的缩放冲击。
		if (Entry.Pulse > 0.0f)
		{
			Entry.Pulse = FMath::Max(0.0f, Entry.Pulse - DeltaTime * 5.0f);
		}

		if (Entry.Elapsed >= Lifetime)
		{
			ActiveEntries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

// 按全局活动数量交替产生左右偏移，不是逐目标统计的独立堆叠槽。
FVector2D UCC_DamageTextSubsystem::NextStackOffset() const
{
	// 同一时刻已有多个飘字时，给新的一个小的水平/垂直错开，
	// 避免密集命中时所有数字叠在完全相同的像素上糊成一团。
	const int32 Slot = ActiveEntries.Num();
	if (Slot == 0)
	{
		return FVector2D::ZeroVector;
	}

	// 按 0, +1, -1, +2, -2 ... 的顺序左右交替，视觉上比单向递增更均衡。
	const int32 Ring = (Slot + 1) / 2;
	const float Direction = (Slot % 2 == 1) ? 1.0f : -1.0f;

	return FVector2D(Direction * Ring * 18.0f, -Ring * 6.0f);
}

// 只更新存活时间与合并脉冲，不投影、不排版，也不修改 GAS 属性。
void UCC_DamageTextSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AgeEntries(DeltaTime);
}