#pragma once

#include "CoreMinimal.h"
#include "AI/Engagement/CC_EngagementTypes.h"
#include "AI/Engagement/Slots/CC_SlotRing.h"
#include "AI/Engagement/Tokens/CC_AttackTokenPool.h"
#include "UObject/ObjectKey.h"

class AActor;

/** 围攻组中的一个攻击者。只由 UCC_EngagementSubsystem 修改。 */
struct FCC_AttackerEntry
{
	TWeakObjectPtr<AActor> Attacker;
	FObjectKey Key;

	ECC_EngagementTier Tier = ECC_EngagementTier::Queue;
	bool bRanged = false;

	/** Engaged 时为内圈槽位索引；Surround 时为外圈（近战）或远程圈（弓兵）槽位索引；其他档位为 INDEX_NONE。 */
	int32 SlotIndex = INDEX_NONE;
	int32 TokenCost = 1;

	/** 分配编号，全局唯一递增，见 FCC_EngagementAssignment::Generation。 */
	int32 Generation = 0;

	/** 早于这个世界时间不会被发放令牌（归还或收回后 = 当时 + AttackDelay）。 */
	float NextEligibleTime = 0.f;

	/** 进展检查：到内圈槽位的最近距离与累计无进展时间。 */
	float BestDistToSlot = TNumericLimits<float>::Max();
	float NoProgressTime = 0.f;

	/** 上次推送给控制器的分配，用于判断是否需要再次通知。 */
	FCC_EngagementAssignment LastNotified;
	bool bHasNotified = false;

	void ResetProgress()
	{
		BestDistToSlot = TNumericLimits<float>::Max();
		NoProgressTime = 0.f;
	}
};

/** 一个被围攻目标的围攻组：三个槽位环 + 一个令牌池 + 攻击者列表。 */
struct FCC_EngagementGroup
{
	TWeakObjectPtr<AActor> Target;

	FCC_SlotRing InnerRing;
	FCC_SlotRing OuterRing;
	FCC_SlotRing RangedRing;
	/** 远程圈半径，由第一个加入的弓兵的射程决定，组存在期间不变，保证槽位索引稳定。 */
	float RangedRadius = 0.f;

	FCC_AttackTokenPool Tokens;
	TArray<FCC_AttackerEntry> Attackers;

	float LastGrantTime = -1.e6f;
	float TimeSinceUpdate = 0.f;
	bool bForceUpdate = true;

	FCC_AttackerEntry* FindEntry(const FObjectKey& Key)
	{
		return Attackers.FindByPredicate([&Key](const FCC_AttackerEntry& Entry) { return Entry.Key == Key; });
	}

	const FCC_AttackerEntry* FindEntry(const FObjectKey& Key) const
	{
		return Attackers.FindByPredicate([&Key](const FCC_AttackerEntry& Entry) { return Entry.Key == Key; });
	}

	/** @return 攻击者当前档位对应的槽位环；Queue / None 返回所在兵种的环（用于计算排队点）。 */
	FCC_SlotRing& RingFor(const FCC_AttackerEntry& Entry)
	{
		if (Entry.Tier == ECC_EngagementTier::Engaged)
		{
			return InnerRing;
		}
		return Entry.bRanged ? RangedRing : OuterRing;
	}

	const FCC_SlotRing& RingFor(const FCC_AttackerEntry& Entry) const
	{
		return const_cast<FCC_EngagementGroup*>(this)->RingFor(Entry);
	}
};
