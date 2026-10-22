#include "Components/CC_SummonBuffShareComponent.h"

#include "Actors/Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffTags.h"

UCC_SummonBuffShareComponent::UCC_SummonBuffShareComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UCC_SummonBuffShareComponent::StartSharing(ACC_BaseCharacter* InSummoner)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(InSummoner) || !InSummoner->GetBuffComponent())
	{
		return;
	}
	Summoner = InSummoner;
	SummonerBuffs = InSummoner->GetBuffComponent();
	SummonerBuffs->OnBuffsChanged.AddUniqueDynamic(this, &ThisClass::HandleSummonerBuffsChanged);
	Seen.Reset();
	SyncNewApplications();
}

void UCC_SummonBuffShareComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCC_BuffComponent* Buffs = SummonerBuffs.Get())
	{
		Buffs->OnBuffsChanged.RemoveDynamic(this, &ThisClass::HandleSummonerBuffsChanged);
	}
	SummonerBuffs.Reset();
	Super::EndPlay(EndPlayReason);
}

void UCC_SummonBuffShareComponent::HandleSummonerBuffsChanged()
{
	SyncNewApplications();
}

void UCC_SummonBuffShareComponent::SyncNewApplications()
{
	const UCC_BuffComponent* Source = SummonerBuffs.Get();
	const ACC_BaseCharacter* Self = Cast<ACC_BaseCharacter>(GetOwner());
	UCC_BuffComponent* Target = Self ? Self->GetBuffComponent() : nullptr;
	if (!Source || !Target)
	{
		return;
	}

	TMap<int32, FSeenBuff> Current;
	for (const FCC_ReplicatedBuffItem& Item : Source->GetActiveBuffs())
	{
		Current.Add(Item.InstanceId, FSeenBuff{Item.StackCount, Item.EndServerTime});
		if (!Item.EffectClass || Item.BuffTags.HasTag(CCBuffTags::NotShared))
		{
			continue;
		}

		// 召唤者这次"又获得了几次"：新 Buff 按当前层数；已有 Buff 看层数增加，层数不变但到期时刻推后视为刷新一次。
		int32 Applications = 0;
		if (const FSeenBuff* Old = Seen.Find(Item.InstanceId))
		{
			if (Item.StackCount > Old->StackCount)
			{
				Applications = Item.StackCount - Old->StackCount;
			}
			else if (Item.EndServerTime > Old->EndServerTime + KINDA_SMALL_NUMBER)
			{
				Applications = 1;
			}
		}
		else
		{
			Applications = FMath::Max(1, Item.StackCount);
		}

		// 召唤物死亡后不再接收 Buff，但仍记录快照，避免之后误判为新增。
		if (!Self->IsAlive())
		{
			continue;
		}
		for (int32 Index = 0; Index < Applications; ++Index)
		{
			Target->ApplyBuff(Item.EffectClass, nullptr, Item.Level);
		}
	}
	Seen = MoveTemp(Current);
}
