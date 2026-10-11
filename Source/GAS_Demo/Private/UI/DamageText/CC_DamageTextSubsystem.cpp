#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"


/**
 * 判断当前世界是否需要本地飘字子系统；专用服务器不创建。
 * @param Outer 子系统所属对象，通常是当前 UWorld。
 * @return 允许创建返回 true，否则返回 false。
 */
bool UCC_DamageTextSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const auto* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->GetNetMode() != NM_DedicatedServer;
}

/**
 * 提供子系统更新开销的性能统计标识。
 * @return 供引擎 stat 系统使用的统计标识。
 */
TStatId UCC_DamageTextSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCC_DamageTextSubsystem, STATGROUP_Tickables);
}

/**
 * 没有活动数字时停止更新，避免空闲帧持续查询。
 * @return 子系统可运行且存在活动条目时返回 true。
 */
bool UCC_DamageTextSubsystem::IsTickable() const
{
	return Super::IsTickable() && !ActiveEntries.IsEmpty();
}

/**
 * 接收 Cue 的本地表现：可选同组累加，按玩家限制数量，再按目标安排初始错位。
 * @param Viewer 本次数字的本地观看者；必须是有效的本地 PlayerController。
 * @param Target 受击或被治疗的目标，用于归属与合并分组。
 * @param Source 攻击或治疗来源，用于归属与合并分组，可为空。
 * @param Location 数字起点的世界坐标；后续移动只影响投影，不自动跟随目标。
 * @param Amount 指定数值；ReportHitForPlayer 中正数表示伤害、负数表示治疗。
 * @param Style 普通、暴击或治疗样式，决定颜色、字号及治疗加号。
 * @param DamageType 伤害分类标签；不同分类不合并，当前不自动计算元素抗性。
 */
void UCC_DamageTextSubsystem::ReportHitForPlayer(APlayerController* Viewer, AActor* Target, AActor* Source,
                                                 const FVector& Location, float Amount, ECC_DamageTextStyle Style,
                                                 FGameplayTag DamageType)
{
	if (!IsValid(Viewer) || !Viewer->IsLocalController() || !FMath::IsFinite(Amount) || FMath::IsNearlyZero(Amount) ||
		Location.ContainsNaN()) return;
	LastMergeCount = 0;
	// 合并必须同时匹配玩家、目标、来源、样式与分类，不能混合伤害和治疗。
	if (MergeWindow > 0.f && IsValid(Target))
	{
		for (auto& Entry : ActiveEntries)
		{
			if (Entry.Viewer != Viewer || Entry.Target != Target || Entry.Source != Source || Entry.Style != Style
				|| Entry.DamageType != DamageType || Entry.Elapsed > MergeWindow
				|| FVector::DistSquared(Entry.WorldLocation, Location) > FMath::Square(MergeDistance))
				continue;
			const float Total = Entry.Amount + Amount;
			if (!FMath::IsFinite(Total)) continue;
			Entry.Amount = Total;
			Entry.Elapsed = 0.f;
			Entry.Pulse = 1.f;
			LastMergeCount = 1;
			OnEntriesUpdated.Broadcast();
			return;
		}
	}
	int32 Count = 0, Slot = 0, Oldest = INDEX_NONE, OldestNormal = INDEX_NONE;
	for (int32 Index = 0; Index < ActiveEntries.Num(); ++Index)
	{
		const auto& Entry = ActiveEntries[Index];
		if (Entry.Viewer != Viewer) continue;
		++Count;
		if (Entry.Target == Target) ++Slot;
		if (Oldest == INDEX_NONE || Entry.Elapsed > ActiveEntries[Oldest].Elapsed) Oldest = Index;
		if (Entry.Style == ECC_DamageTextStyle::Normal && (OldestNormal == INDEX_NONE || Entry.Elapsed > ActiveEntries[
			OldestNormal].Elapsed)) OldestNormal = Index;
	}
	// 容量不足时优先移除普通数字；重要数字已经占满时可丢弃新的普通数字。
	if (Count >= FMath::Max(1, MaxEntriesPerPlayer))
	{
		if (OldestNormal == INDEX_NONE && Style == ECC_DamageTextStyle::Normal) return;
		ActiveEntries.RemoveAt(OldestNormal != INDEX_NONE ? OldestNormal : Oldest);
	}
	FCC_DamageTextEntry Entry;
	Entry.Id = NextEntryId++;
	Entry.Viewer = Viewer;
	Entry.Target = Target;
	Entry.Source = Source;
	Entry.DamageType = DamageType;
	Entry.WorldLocation = Location;
	Entry.Amount = Amount;
	Entry.Style = Style;
	// 按当前目标安排有限的左右扇形槽位，其他敌人的数字不会改变本目标的偏移。
	Slot %= 7;
	const int32 Ring = (Slot + 1) / 2;
	Entry.StackOffset = FVector2D((Slot % 2 ? 1.f : -1.f) * Ring * 18.f, -Ring * 6.f);
	ActiveEntries.Insert(Entry, 0);
	OnEntriesUpdated.Broadcast();
}

/**
 * 更新数字年龄与合并脉冲，回收过期条目，并通知画布重绘。
 * @param DeltaTime 本次更新经过的秒数，用于年龄和脉冲衰减。
 */
void UCC_DamageTextSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (int32 Index = ActiveEntries.Num() - 1; Index >= 0; --Index)
	{
		auto& Entry = ActiveEntries[Index];
		Entry.Elapsed += DeltaTime;
		Entry.Pulse = FMath::Max(0.f, Entry.Pulse - DeltaTime * 5.f);
		if (!Entry.Viewer.IsValid() || Entry.Elapsed >= FMath::Max(0.1f, Lifetime)) ActiveEntries.RemoveAt(Index);
	}
	OnEntriesUpdated.Broadcast();
}

/**
 * 登记玩家的飘字绘制层，用于发现 RootLayout 与 HUD 重复挂载。
 * @param Player 当前本地玩家，用于分屏归属和绘制层登记。
 * @return 该玩家登记后的绘制层数量；大于 1 表示可能重复绘制。
 */
int32 UCC_DamageTextSubsystem::AddDrawLayer(const APlayerController* Player)
{
	return ++DrawLayerCounts.FindOrAdd(Player);
}

/**
 * 撤销玩家的绘制层登记。
 * @param Player 当前本地玩家，用于分屏归属和绘制层登记。
 */
void UCC_DamageTextSubsystem::RemoveDrawLayer(const APlayerController* Player)
{
	if (auto* Count = DrawLayerCounts.Find(Player)) { if (--*Count <= 0) DrawLayerCounts.Remove(Player); }
}
