#include "AI/Engagement/CC_EngagementSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayTags/CC_Tags.h"
#include "GAS_Demo.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "Utils/Team/CC_TeamStatics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEngagementDebug(
		TEXT("cc.Engagement.Debug"), 0,
		TEXT("Draw engagement slots and token holders. 0 = off, 1 = on."),
		ECVF_Default);
}

#pragma region 生命周期

bool UCC_EngagementSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// 只在游戏 World 中创建。不在这里判断 NetMode：子系统创建时客户端的网络驱动可能还没就绪；
	// 客户端没有 AIController，也就不会有任何注册，Tick 会立即返回。
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UCC_EngagementSubsystem::Deinitialize()
{
	Groups.Empty();
	AttackerToTarget.Empty();
	PendingNotifies.Empty();
	OnAssignmentChanged.Clear();
	Super::Deinitialize();
}

TStatId UCC_EngagementSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCC_EngagementSubsystem, STATGROUP_Tickables);
}

void UCC_EngagementSubsystem::Tick(float DeltaTime)
{
	if (Groups.IsEmpty())
	{
		return;
	}

	const float Interval = FMath::Max(0.05f, Settings.UpdateInterval);
	TArray<FObjectKey> EmptyGroups;
	for (TPair<FObjectKey, FCC_EngagementGroup>& Pair : Groups)
	{
		FCC_EngagementGroup& Group = Pair.Value;
		Group.TimeSinceUpdate += DeltaTime;
		if (Group.bForceUpdate || Group.TimeSinceUpdate >= Interval)
		{
			UpdateGroup(Group, Group.TimeSinceUpdate);
			Group.TimeSinceUpdate = 0.f;
			Group.bForceUpdate = false;
		}
		if (Group.Attackers.IsEmpty())
		{
			EmptyGroups.Add(Pair.Key);
		}
	}
	for (const FObjectKey& Key : EmptyGroups)
	{
		Groups.Remove(Key);
	}
	FlushNotifies();
}

#pragma endregion

#pragma region 对外接口

/**
 * 
 * @param Attacker 
 * @param Target 
 * @param bRanged 
 */
void UCC_EngagementSubsystem::RegisterAttacker(AActor* Attacker, AActor* Target, bool bRanged)
{
	if (!IsValid(Attacker))
	{
		return;
	}
	if (!IsValid(Target) || Attacker == Target)
	{
		UnregisterAttacker(Attacker);
		return;
	}

	const FObjectKey AttackerKey(Attacker);
	const FObjectKey TargetKey(Target);
	if (const FObjectKey* CurrentTarget = AttackerToTarget.Find(AttackerKey))
	{
		if (*CurrentTarget == TargetKey)
		{
			// 已在同一个组中：只同步兵种，不重置任何状态。
			if (FCC_EngagementGroup* Group = Groups.Find(TargetKey))
			{
				if (FCC_AttackerEntry* Entry = Group->FindEntry(AttackerKey))
				{
					Entry->bRanged = bRanged;
				}
			}
			return;
		}
		// 换目标：先从旧组静默移出，紧接着会通知新组的分配，避免黑板闪一次 None。
		RemoveAttacker(AttackerKey, false);
	}

	FCC_EngagementGroup* Group = Groups.Find(TargetKey);
	if (!Group)
	{
		Group = &Groups.Add(TargetKey);
		Group->Target = Target;
		Group->Tokens.Capacity = FMath::Max(1, Settings.TokenCapacity);
		// 各组的更新起点随机错开，避免所有组挤在同一帧。
		Group->TimeSinceUpdate = FMath::FRand() * Settings.UpdateInterval;
	}
	if (bRanged && Group->RangedRadius <= 0.f)
	{
		Group->RangedRadius = ComputeRangedRadius(Attacker);
	}

	FCC_AttackerEntry& Entry = Group->Attackers.AddDefaulted_GetRef();
	Entry.Attacker = Attacker;
	Entry.Key = AttackerKey;
	Entry.bRanged = bRanged;
	Entry.Tier = ECC_EngagementTier::Queue;
	Entry.Generation = AllocateGeneration();
	Entry.NextEligibleTime = GetNow();
	Group->bForceUpdate = true;
	AttackerToTarget.Add(AttackerKey, TargetKey);

	UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Engagement] Register: Attacker=%s Target=%s Ranged=%d Generation=%d"),
		*GetNameSafe(Attacker), *GetNameSafe(Target), bRanged, Entry.Generation);

	NotifyIfChanged(*Group, Entry, true);
	FlushNotifies();
}

void UCC_EngagementSubsystem::UnregisterAttacker(AActor* Attacker)
{
	if (!Attacker)
	{
		return;
	}
	RemoveAttacker(FObjectKey(Attacker), true);
	FlushNotifies();
}

void UCC_EngagementSubsystem::ReleaseToken(AActor* Attacker, int32 Generation)
{
	if (!IsValid(Attacker))
	{
		return;
	}
	const FObjectKey Key(Attacker);
	FCC_EngagementGroup* Group = FindGroupOf(Key);
	FCC_AttackerEntry* Entry = Group ? Group->FindEntry(Key) : nullptr;
	if (!Entry)
	{
		return;
	}
	if (Entry->Tier != ECC_EngagementTier::Engaged || !Group->Tokens.Holds(Key) || Entry->Generation != Generation)
	{
		// 弓兵射击、旧攻击在换组后结束等情况都会走到这里，属于正常忽略。
		UE_LOG(LogGAS_Demo, Verbose,
			TEXT("[AI][Engagement] Release ignored: Attacker=%s Tier=%d HoldsToken=%d Generation=%d Current=%d"),
			*GetNameSafe(Attacker), static_cast<int32>(Entry->Tier), Group->Tokens.Holds(Key), Generation,
			Entry->Generation);
		return;
	}

	const float Now = GetNow();
	Group->InnerRing.Vacate(Entry->SlotIndex);
	Group->Tokens.Release(Key);
	Entry->Tier = ECC_EngagementTier::Surround;
	Entry->SlotIndex = INDEX_NONE;
	Entry->Generation = AllocateGeneration();
	Entry->NextEligibleTime = Now + GetAttackDelay(Attacker);
	Entry->ResetProgress();
	PlaceInSurroundRing(*Group, *Entry);

	UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Engagement] Token released: Attacker=%s NextEligible=%.2f Generation=%d"),
		*GetNameSafe(Attacker), Entry->NextEligibleTime, Entry->Generation);

	NotifyIfChanged(*Group, *Entry, true);
	FlushNotifies();
}

FCC_EngagementAssignment UCC_EngagementSubsystem::GetAssignment(const AActor* Attacker) const
{
	if (!Attacker)
	{
		return FCC_EngagementAssignment();
	}
	const FObjectKey Key(Attacker);
	const FCC_EngagementGroup* Group = FindGroupOf(Key);
	const FCC_AttackerEntry* Entry = Group ? Group->FindEntry(Key) : nullptr;
	return Entry ? BuildAssignment(*Group, *Entry) : FCC_EngagementAssignment();
}

#pragma endregion

#pragma region 内部工具

float UCC_EngagementSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

int32 UCC_EngagementSubsystem::AllocateGeneration()
{
	const int32 Result = NextGeneration;
	NextGeneration = NextGeneration == MAX_int32 ? 1 : NextGeneration + 1;
	return Result;
}

FCC_EngagementGroup* UCC_EngagementSubsystem::FindGroupOf(const FObjectKey& AttackerKey)
{
	const FObjectKey* TargetKey = AttackerToTarget.Find(AttackerKey);
	return TargetKey ? Groups.Find(*TargetKey) : nullptr;
}

const FCC_EngagementGroup* UCC_EngagementSubsystem::FindGroupOf(const FObjectKey& AttackerKey) const
{
	const FObjectKey* TargetKey = AttackerToTarget.Find(AttackerKey);
	return TargetKey ? Groups.Find(*TargetKey) : nullptr;
}

void UCC_EngagementSubsystem::RemoveAttacker(const FObjectKey& AttackerKey, bool bNotify)
{
	const FObjectKey* TargetKeyPtr = AttackerToTarget.Find(AttackerKey);
	if (!TargetKeyPtr)
	{
		return;
	}
	const FObjectKey TargetKey = *TargetKeyPtr;
	AttackerToTarget.Remove(AttackerKey);

	FCC_EngagementGroup* Group = Groups.Find(TargetKey);
	if (!Group)
	{
		return;
	}
	const int32 Index = Group->Attackers.IndexOfByPredicate(
		[&AttackerKey](const FCC_AttackerEntry& Entry) { return Entry.Key == AttackerKey; });
	if (Index != INDEX_NONE)
	{
		FCC_AttackerEntry& Entry = Group->Attackers[Index];
		if (Entry.Tier == ECC_EngagementTier::Engaged || Entry.Tier == ECC_EngagementTier::Surround)
		{
			Group->RingFor(Entry).Vacate(Entry.SlotIndex);
		}
		Group->Tokens.Release(AttackerKey);
		if (bNotify && Entry.Attacker.IsValid())
		{
			PendingNotifies.Emplace(Entry.Attacker, FCC_EngagementAssignment());
		}
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Engagement] Unregister: Attacker=%s Target=%s"),
			*GetNameSafe(Entry.Attacker.Get()), *GetNameSafe(Group->Target.Get()));
		Group->Attackers.RemoveAtSwap(Index);
	}
	if (Group->Attackers.IsEmpty())
	{
		Groups.Remove(TargetKey);
	}
}

float UCC_EngagementSubsystem::ComputeRangedRadius(const AActor* Attacker) const
{
	const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Attacker);
	const UCC_CharacterConfig* Config = Character ? Character->GetCharacterConfig() : nullptr;
	if (!Config || Config->MaxRange <= 0.f)
	{
		return Settings.RangedRadiusFallback;
	}
	// 站在射程中线，同时保证在近战外圈之外，不和近战抢位置。
	const float MidRange = (Config->MinRange + Config->MaxRange) * 0.5f;
	return FMath::Max(MidRange, Settings.OuterRadius + Settings.SlotSpacing);
}

float UCC_EngagementSubsystem::GetAttackDelay(const AActor* Attacker)
{
	const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Attacker);
	const UCC_CharacterConfig* Config = Character ? Character->GetCharacterConfig() : nullptr;
	return Config ? FMath::Max(0.f, Config->AttackDelay) : 1.f;
}

bool UCC_EngagementSubsystem::IsAttacking(const AActor* Attacker)
{
	const UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Attacker));
	return ASC && ASC->HasMatchingGameplayTag(CCTags::CCAbilities::Attack);
}

#pragma endregion

#pragma region 周期更新

void UCC_EngagementSubsystem::UpdateGroup(FCC_EngagementGroup& Group, float GroupDeltaTime)
{
	PruneInvalid(Group);
	if (Group.Attackers.IsEmpty() || !Group.Target.IsValid())
	{
		return;
	}

	const float Now = GetNow();
	Group.Tokens.Capacity = FMath::Max(1, Settings.TokenCapacity);
	RebuildRings(Group, Now);
	CheckProgress(Group, GroupDeltaTime, Now);
	AssignSlots(Group, Now);
	TryGrantToken(Group, Now);

	for (FCC_AttackerEntry& Entry : Group.Attackers)
	{
		NotifyIfChanged(Group, Entry, false);
	}

	if (CVarEngagementDebug.GetValueOnGameThread() > 0)
	{
		DrawDebug(Group);
	}
}

/**
 * 移除已销毁、已死亡、不再敌对的攻击者；目标死亡时整组清空。
 * 控制器的注销路径漏掉时，这里是兜底：令牌最多泄漏一个更新周期。
 */
void UCC_EngagementSubsystem::PruneInvalid(FCC_EngagementGroup& Group)
{
	AActor* Target = Group.Target.Get();
	const ACC_BaseCharacter* TargetCharacter = Cast<ACC_BaseCharacter>(Target);
	const bool bTargetGone = !IsValid(Target) || (TargetCharacter && !TargetCharacter->IsAlive());

	for (int32 Index = Group.Attackers.Num() - 1; Index >= 0; --Index)
	{
		FCC_AttackerEntry& Entry = Group.Attackers[Index];
		AActor* Attacker = Entry.Attacker.Get();
		bool bInvalid = bTargetGone || !IsValid(Attacker);
		if (!bInvalid)
		{
			const ACC_BaseCharacter* AttackerCharacter = Cast<ACC_BaseCharacter>(Attacker);
			bInvalid = (AttackerCharacter && !AttackerCharacter->IsAlive())
				|| !UCC_TeamStatics::IsHostile(Attacker, Target);
		}
		if (!bInvalid)
		{
			continue;
		}
		Group.Tokens.Release(Entry.Key);
		AttackerToTarget.Remove(Entry.Key);
		if (IsValid(Attacker))
		{
			PendingNotifies.Emplace(Attacker, FCC_EngagementAssignment());
		}
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Engagement] Pruned: Attacker=%s Target=%s TargetGone=%d"),
			*GetNameSafe(Attacker), *GetNameSafe(Target), bTargetGone);
		Group.Attackers.RemoveAtSwap(Index);
	}
	Group.Tokens.PruneIf([&Group](const FObjectKey& Key) { return Group.FindEntry(Key) == nullptr; });
}

void UCC_EngagementSubsystem::RebuildRings(FCC_EngagementGroup& Group, float Now)
{
	const AActor* Target = Group.Target.Get();
	const FVector Center = Target->GetActorLocation();

	bool bHasMelee = false;
	bool bHasRanged = false;
	for (const FCC_AttackerEntry& Entry : Group.Attackers)
	{
		bHasMelee |= !Entry.bRanged;
		bHasRanged |= Entry.bRanged;
	}

	// 目标在 NavMesh 上的投影点，用作「墙后槽位」射线检查的起点。
	FVector NavCenter = Center;
	bool bHasNavCenter = false;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (Nav->ProjectPointToNavigation(Center, Projected, Settings.ProjectExtent))
		{
			NavCenter = Projected.Location;
			bHasNavCenter = true;
		}
	}

	if (bHasMelee)
	{
		Group.InnerRing.Rebuild(Center, Settings.InnerRadius, Settings.SlotSpacing, Settings.SlotJitter, Now);
		ValidateRing(Group.InnerRing, NavCenter, bHasNavCenter);
		Group.OuterRing.Rebuild(Center, Settings.OuterRadius, Settings.SlotSpacing, Settings.SlotJitter, Now);
		ValidateRing(Group.OuterRing, NavCenter, bHasNavCenter);
	}
	if (bHasRanged)
	{
		if (Group.RangedRadius <= 0.f)
		{
			Group.RangedRadius = Settings.RangedRadiusFallback;
		}
		Group.RangedRing.Rebuild(Center, Group.RangedRadius, Settings.SlotSpacing, Settings.SlotJitter, Now);
		ValidateRing(Group.RangedRing, NavCenter, bHasNavCenter);
	}
}

/**
 * 槽位可用需要同时满足：不在冷却中（Rebuild 已处理）、投影到 NavMesh 成功、
 * 从目标到槽位的 NavMesh 射线没有被挡（排除薄墙另一侧的槽位）。没有导航系统时保留原始位置。
 */
void UCC_EngagementSubsystem::ValidateRing(FCC_SlotRing& Ring, const FVector& NavCenter, bool bHasNavCenter) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav)
	{
		return;
	}
	for (FCC_Slot& Slot : Ring.Slots)
	{
		if (!Slot.bUsable)
		{
			continue;
		}
		FNavLocation Projected;
		if (!Nav->ProjectPointToNavigation(Slot.WorldLocation, Projected, Settings.ProjectExtent))
		{
			Slot.bUsable = false;
			continue;
		}
		Slot.WorldLocation = Projected.Location;
		FVector HitLocation;
		if (bHasNavCenter && UNavigationSystemV1::NavigationRaycast(World, NavCenter, Projected.Location, HitLocation))
		{
			Slot.bUsable = false;
		}
	}
}

/**
 * 进展检查，只看 Engaged：
 * 攻击中 / 有进展 → 清零；未到达且目标在快速移动 → 不计时；其他（含到达后未出手）→ 累计。
 * 超时收回令牌；只有未到达且自身几乎静止（真的卡住）时才冷却该内圈槽位。
 */
void UCC_EngagementSubsystem::CheckProgress(FCC_EngagementGroup& Group, float GroupDeltaTime, float Now)
{
	const AActor* Target = Group.Target.Get();
	const float TargetSpeed = Target ? Target->GetVelocity().Size2D() : 0.f;

	for (FCC_AttackerEntry& Entry : Group.Attackers)
	{
		if (Entry.Tier != ECC_EngagementTier::Engaged || !Group.InnerRing.Slots.IsValidIndex(Entry.SlotIndex))
		{
			continue;
		}
		const AActor* Attacker = Entry.Attacker.Get();
		if (!Attacker)
		{
			continue;
		}

		const float Dist = FVector::Dist2D(Attacker->GetActorLocation(),
			Group.InnerRing.Slots[Entry.SlotIndex].WorldLocation);
		const bool bArrived = Dist <= Settings.ArriveRadius;

		if (IsAttacking(Attacker))
		{
			Entry.NoProgressTime = 0.f;
			Entry.BestDistToSlot = Dist;
			continue;
		}
		if (Dist < Entry.BestDistToSlot - Settings.ProgressStep)
		{
			Entry.BestDistToSlot = Dist;
			Entry.NoProgressTime = 0.f;
			continue;
		}
		if (!bArrived && TargetSpeed > Settings.TargetFastSpeed)
		{
			continue;
		}

		Entry.NoProgressTime += GroupDeltaTime;
		if (Entry.NoProgressTime >= Settings.StuckTimeout)
		{
			const bool bCooldownSlot = !bArrived && Attacker->GetVelocity().Size2D() < Settings.StuckSelfSpeed;
			UE_LOG(LogGAS_Demo, Log,
				TEXT("[AI][Engagement] No progress, revoke: Attacker=%s Dist=%.1f Arrived=%d CooldownSlot=%d"),
				*GetNameSafe(Attacker), Dist, bArrived, bCooldownSlot);
			RevokeToken(Group, Entry, bCooldownSlot, Now);
		}
	}
}

void UCC_EngagementSubsystem::AssignSlots(FCC_EngagementGroup& Group, float Now)
{
	const FVector Center = Group.Target->GetActorLocation();

	// 1. 持令牌者 → 内圈。没有可用内圈槽位的收回令牌（不冷却），回到下面的近战包围分配。
	{
		TArray<int32> EntryIndices;
		TArray<FVector> Positions;
		TArray<int32> SlotIndices;
		for (int32 Index = 0; Index < Group.Attackers.Num(); ++Index)
		{
			const FCC_AttackerEntry& Entry = Group.Attackers[Index];
			if (Entry.Tier == ECC_EngagementTier::Engaged && Entry.Attacker.IsValid())
			{
				EntryIndices.Add(Index);
				Positions.Add(Entry.Attacker->GetActorLocation());
				SlotIndices.Add(Entry.SlotIndex);
			}
		}
		Group.InnerRing.AssignIncremental(Center, Positions, SlotIndices);
		for (int32 Index = 0; Index < EntryIndices.Num(); ++Index)
		{
			FCC_AttackerEntry& Entry = Group.Attackers[EntryIndices[Index]];
			if (SlotIndices[Index] == INDEX_NONE)
			{
				RevokeToken(Group, Entry, false, Now);
			}
			else if (SlotIndices[Index] != Entry.SlotIndex)
			{
				Entry.SlotIndex = SlotIndices[Index];
				Entry.ResetProgress();
			}
		}
	}

	// 2. 其余近战 → 外圈；3. 弓兵 → 远程圈。分不到槽位的降为排队。
	auto AssignRing = [&Group, &Center](FCC_SlotRing& Ring, bool bRanged)
	{
		TArray<int32> EntryIndices;
		TArray<FVector> Positions;
		TArray<int32> SlotIndices;
		for (int32 Index = 0; Index < Group.Attackers.Num(); ++Index)
		{
			const FCC_AttackerEntry& Entry = Group.Attackers[Index];
			if (Entry.bRanged == bRanged && Entry.Tier != ECC_EngagementTier::Engaged && Entry.Attacker.IsValid())
			{
				EntryIndices.Add(Index);
				Positions.Add(Entry.Attacker->GetActorLocation());
				SlotIndices.Add(Entry.Tier == ECC_EngagementTier::Surround ? Entry.SlotIndex : INDEX_NONE);
			}
		}
		if (EntryIndices.IsEmpty())
		{
			return;
		}
		Ring.AssignIncremental(Center, Positions, SlotIndices);
		for (int32 Index = 0; Index < EntryIndices.Num(); ++Index)
		{
			FCC_AttackerEntry& Entry = Group.Attackers[EntryIndices[Index]];
			Entry.SlotIndex = SlotIndices[Index];
			Entry.Tier = Entry.SlotIndex != INDEX_NONE ? ECC_EngagementTier::Surround : ECC_EngagementTier::Queue;
		}
	};
	AssignRing(Group.OuterRing, false);
	AssignRing(Group.RangedRing, true);
}

/**
 * 每次更新最多发一个令牌。条件：距上次发放 ≥ MinGrantInterval、令牌有余量、内圈有空闲可用槽位。
 * 候选人：近战、Surround、冷却已过、不在攻击中；选离最近空闲内圈槽位最近的那个。
 * 发令牌与占内圈槽位在同一步完成。
 */
void UCC_EngagementSubsystem::TryGrantToken(FCC_EngagementGroup& Group, float Now)
{
	if (Now - Group.LastGrantTime < Settings.MinGrantInterval)
	{
		return;
	}

	FCC_AttackerEntry* Best = nullptr;
	int32 BestSlot = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	for (FCC_AttackerEntry& Entry : Group.Attackers)
	{
		if (Entry.bRanged || Entry.Tier != ECC_EngagementTier::Surround || Now < Entry.NextEligibleTime
			|| Group.Tokens.Available() < Entry.TokenCost)
		{
			continue;
		}
		const AActor* Attacker = Entry.Attacker.Get();
		if (!Attacker || IsAttacking(Attacker))
		{
			continue;
		}
		const FVector Location = Attacker->GetActorLocation();
		const int32 Slot = Group.InnerRing.FindFreeSlotNear(Location);
		if (Slot == INDEX_NONE)
		{
			// 空闲槽位与候选人无关：一个都找不到，说明内圈已满或被地形挡住。
			return;
		}
		const float DistSq = FVector::DistSquared2D(Location, Group.InnerRing.Slots[Slot].WorldLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = &Entry;
			BestSlot = Slot;
		}
	}

	if (!Best || !Group.Tokens.TryAcquire(Best->Key, Best->TokenCost))
	{
		return;
	}
	Group.OuterRing.Vacate(Best->SlotIndex);
	Group.InnerRing.Occupy(BestSlot);
	Best->Tier = ECC_EngagementTier::Engaged;
	Best->SlotIndex = BestSlot;
	Best->Generation = AllocateGeneration();
	Best->ResetProgress();
	Group.LastGrantTime = Now;

	UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Engagement] Token granted: Attacker=%s Target=%s Slot=%d Generation=%d Used=%d/%d"),
		*GetNameSafe(Best->Attacker.Get()), *GetNameSafe(Group.Target.Get()), BestSlot, Best->Generation,
		Group.Tokens.Used(), Group.Tokens.Capacity);
}

void UCC_EngagementSubsystem::RevokeToken(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry, bool bCooldownSlot,
	float Now)
{
	if (bCooldownSlot && Group.InnerRing.Slots.IsValidIndex(Entry.SlotIndex))
	{
		FCC_Slot& Slot = Group.InnerRing.Slots[Entry.SlotIndex];
		Slot.CooldownUntil = Now + Settings.SlotCooldown;
		Slot.bUsable = false;
	}
	Group.InnerRing.Vacate(Entry.SlotIndex);
	Group.Tokens.Release(Entry.Key);
	Entry.Tier = ECC_EngagementTier::Surround;
	Entry.SlotIndex = INDEX_NONE;
	Entry.Generation = AllocateGeneration();
	Entry.NextEligibleTime = Now + GetAttackDelay(Entry.Attacker.Get());
	Entry.ResetProgress();
	PlaceInSurroundRing(Group, Entry);
}

void UCC_EngagementSubsystem::PlaceInSurroundRing(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry) const
{
	FCC_SlotRing& Ring = Entry.bRanged ? Group.RangedRing : Group.OuterRing;
	const AActor* Attacker = Entry.Attacker.Get();
	const int32 Slot = Attacker ? Ring.FindFreeSlotNear(Attacker->GetActorLocation()) : INDEX_NONE;
	if (Slot != INDEX_NONE)
	{
		Ring.Occupy(Slot);
		Entry.Tier = ECC_EngagementTier::Surround;
		Entry.SlotIndex = Slot;
	}
	else
	{
		Entry.Tier = ECC_EngagementTier::Queue;
		Entry.SlotIndex = INDEX_NONE;
	}
}

#pragma endregion

#pragma region 分配与通知

FCC_EngagementAssignment UCC_EngagementSubsystem::BuildAssignment(const FCC_EngagementGroup& Group,
	const FCC_AttackerEntry& Entry) const
{
	FCC_EngagementAssignment Result;
	Result.Tier = Entry.Tier;
	Result.Generation = Entry.Generation;
	Result.bHasToken = Entry.Tier == ECC_EngagementTier::Engaged;

	const AActor* Attacker = Entry.Attacker.Get();
	const AActor* Target = Group.Target.Get();
	switch (Entry.Tier)
	{
	case ECC_EngagementTier::Engaged:
	case ECC_EngagementTier::Surround:
	{
		const FCC_SlotRing& Ring = Group.RingFor(Entry);
		if (Ring.Slots.IsValidIndex(Entry.SlotIndex))
		{
			Result.MoveLocation = Ring.Slots[Entry.SlotIndex].WorldLocation;
			Result.bLocationValid = true;
		}
		break;
	}
	case ECC_EngagementTier::Queue:
	{
		if (Attacker && Target)
		{
			// 排队点：沿自身方位停在所在环外侧 QueueMargin；已在更远处就原地等待。
			const float RingRadius = Entry.bRanged
				? (Group.RangedRadius > 0.f ? Group.RangedRadius : Settings.RangedRadiusFallback)
				: Settings.OuterRadius;
			const float QueueDistance = RingRadius + Settings.QueueMargin;
			const FVector Center = Target->GetActorLocation();
			const FVector Location = Attacker->GetActorLocation();
			if (FVector::Dist2D(Location, Center) >= QueueDistance)
			{
				Result.MoveLocation = Location;
			}
			else
			{
				FVector Direction = (Location - Center).GetSafeNormal2D();
				if (Direction.IsNearlyZero())
				{
					Direction = Attacker->GetActorForwardVector().GetSafeNormal2D();
				}
				if (Direction.IsNearlyZero())
				{
					Direction = FVector::ForwardVector;
				}
				Result.MoveLocation = Center + Direction * QueueDistance;
				Result.MoveLocation.Z = Location.Z;
			}
			Result.bLocationValid = true;
		}
		break;
	}
	default:
		break;
	}
	return Result;
}

void UCC_EngagementSubsystem::NotifyIfChanged(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry, bool bForce)
{
	const FCC_EngagementAssignment Current = BuildAssignment(Group, Entry);
	const FCC_EngagementAssignment& Last = Entry.LastNotified;
	const bool bChanged = bForce || !Entry.bHasNotified
		|| Current.Tier != Last.Tier
		|| Current.Generation != Last.Generation
		|| Current.bHasToken != Last.bHasToken
		|| Current.bLocationValid != Last.bLocationValid
		|| FVector::DistSquared(Current.MoveLocation, Last.MoveLocation) > FMath::Square(Settings.NotifyDistance);
	if (!bChanged)
	{
		return;
	}
	Entry.LastNotified = Current;
	Entry.bHasNotified = true;
	PendingNotifies.Emplace(Entry.Attacker, Current);
}

void UCC_EngagementSubsystem::FlushNotifies()
{
	if (PendingNotifies.IsEmpty())
	{
		return;
	}
	// 先移出再广播：回调中再次注册 / 注销时会产生新的通知，由嵌套的 Flush 处理。
	TArray<TPair<TWeakObjectPtr<AActor>, FCC_EngagementAssignment>> Batch = MoveTemp(PendingNotifies);
	PendingNotifies.Reset();
	for (const TPair<TWeakObjectPtr<AActor>, FCC_EngagementAssignment>& Item : Batch)
	{
		if (AActor* Attacker = Item.Key.Get())
		{
			OnAssignmentChanged.Broadcast(Attacker, Item.Value);
		}
	}
}

#pragma endregion

#pragma region 调试

void UCC_EngagementSubsystem::DrawDebug(const FCC_EngagementGroup& Group) const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	const AActor* Target = Group.Target.Get();
	if (!World || !Target)
	{
		return;
	}
	const float LifeTime = Settings.UpdateInterval + 0.05f;

	// 红 = 不可用（投影失败 / 被墙挡 / 冷却），绿 = 空闲，其他颜色 = 已占用。
	auto DrawRing = [World, LifeTime](const FCC_SlotRing& Ring, const FColor& OccupiedColor)
	{
		for (const FCC_Slot& Slot : Ring.Slots)
		{
			const FColor Color = !Slot.bUsable ? FColor::Red : (Slot.bOccupied ? OccupiedColor : FColor::Green);
			DrawDebugSphere(World, Slot.WorldLocation, 18.f, 8, Color, false, LifeTime);
		}
	};
	DrawRing(Group.InnerRing, FColor::Orange);
	DrawRing(Group.OuterRing, FColor::Yellow);
	DrawRing(Group.RangedRing, FColor::Cyan);

	for (const FCC_AttackerEntry& Entry : Group.Attackers)
	{
		const AActor* Attacker = Entry.Attacker.Get();
		if (Attacker && Entry.Tier == ECC_EngagementTier::Engaged && Group.InnerRing.Slots.IsValidIndex(Entry.SlotIndex))
		{
			DrawDebugLine(World, Attacker->GetActorLocation(), Group.InnerRing.Slots[Entry.SlotIndex].WorldLocation,
				FColor::Orange, false, LifeTime, 0, 2.f);
		}
	}
	DrawDebugString(World, Target->GetActorLocation() + FVector(0.f, 0.f, 140.f),
		FString::Printf(TEXT("Tokens %d/%d  Attackers %d"), Group.Tokens.Used(), Group.Tokens.Capacity,
			Group.Attackers.Num()),
		nullptr, FColor::White, LifeTime);
#endif
}

#pragma endregion
