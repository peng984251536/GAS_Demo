#include "AI/Engagement/Slots/CC_SlotRing.h"

#include "Math/RandomStream.h"

int32 FCC_SlotRing::ComputeSlotCount(float InRadius, float InSpacing)
{
	if (InRadius <= KINDA_SMALL_NUMBER || InSpacing <= KINDA_SMALL_NUMBER)
	{
		return 1;
	}
	return FMath::Max(1, FMath::FloorToInt(2.f * UE_PI * InRadius / InSpacing));
}

bool FCC_SlotRing::Rebuild(const FVector& Center, float InRadius, float InSpacing, float Jitter, float Now)
{
	const bool bRegenerate = Slots.Num() == 0
		|| !FMath::IsNearlyEqual(Radius, InRadius)
		|| !FMath::IsNearlyEqual(Spacing, InSpacing);
	if (bRegenerate)
	{
		Radius = InRadius;
		Spacing = InSpacing;
		const int32 Count = ComputeSlotCount(InRadius, InSpacing);
		Slots.Reset(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = 2.f * UE_PI * Index / Count;
			const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			const FVector Tangent(-Radial.Y, Radial.X, 0.f);
			// 固定种子：同一个槽位每次生成的偏移相同，位置稳定。
			FRandomStream Stream(1337 + Index * 7919);
			const float TangentJitter = Stream.FRandRange(-Jitter, Jitter);
			const float RadialJitter = Stream.FRandRange(-Jitter, Jitter) * 0.5f;

			FCC_Slot& Slot = Slots.AddDefaulted_GetRef();
			Slot.LocalOffset = Radial * (InRadius + RadialJitter) + Tangent * TangentJitter;
		}
	}

	for (FCC_Slot& Slot : Slots)
	{
		Slot.WorldLocation = Center + Slot.LocalOffset;
		Slot.bUsable = Now >= Slot.CooldownUntil;
	}
	return bRegenerate;
}

void FCC_SlotRing::ClearOccupancy()
{
	for (FCC_Slot& Slot : Slots)
	{
		Slot.bOccupied = false;
	}
}

bool FCC_SlotRing::IsFree(int32 Index) const
{
	return Slots.IsValidIndex(Index) && Slots[Index].bUsable && !Slots[Index].bOccupied;
}

void FCC_SlotRing::Occupy(int32 Index)
{
	if (Slots.IsValidIndex(Index))
	{
		Slots[Index].bOccupied = true;
	}
}

void FCC_SlotRing::Vacate(int32 Index)
{
	if (Slots.IsValidIndex(Index))
	{
		Slots[Index].bOccupied = false;
	}
}

int32 FCC_SlotRing::CountFreeUsable() const
{
	int32 Count = 0;
	for (const FCC_Slot& Slot : Slots)
	{
		Count += (Slot.bUsable && !Slot.bOccupied) ? 1 : 0;
	}
	return Count;
}

int32 FCC_SlotRing::FindFreeSlotNear(const FVector& Position) const
{
	int32 Best = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!IsFree(Index))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Position, Slots[Index].WorldLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Index;
		}
	}
	return Best;
}

void FCC_SlotRing::AssignIncremental(const FVector& Center, TConstArrayView<FVector> Positions,
	TArray<int32>& InOutSlotIndices)
{
	check(Positions.Num() == InOutSlotIndices.Num());
	ClearOccupancy();

	// 第一步：保留仍然合适的旧槽位。
	TArray<int32> Pending;
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const int32 SlotIndex = InOutSlotIndices[Index];
		if (IsFree(SlotIndex))
		{
			const FVector ToAttacker = (Positions[Index] - Center).GetSafeNormal2D();
			const FVector ToSlot = Slots[SlotIndex].LocalOffset.GetSafeNormal2D();
			// 夹角不超过 90°：槽位仍在攻击者这一侧，不需要绕到目标另一边。
			if (ToAttacker.IsNearlyZero() || FVector::DotProduct(ToAttacker, ToSlot) >= 0.f)
			{
				Occupy(SlotIndex);
				continue;
			}
		}
		InOutSlotIndices[Index] = INDEX_NONE;
		Pending.Add(Index);
	}

	// 第二步：离中心近的先挑，各自挑最近的空位。
	Pending.Sort([&Positions, &Center](int32 A, int32 B)
	{
		return FVector::DistSquared2D(Positions[A], Center) < FVector::DistSquared2D(Positions[B], Center);
	});
	for (const int32 Index : Pending)
	{
		const int32 SlotIndex = FindFreeSlotNear(Positions[Index]);
		if (SlotIndex != INDEX_NONE)
		{
			Occupy(SlotIndex);
		}
		InOutSlotIndices[Index] = SlotIndex;
	}
}
