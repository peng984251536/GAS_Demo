#include "AI/Engagement/Tokens/CC_AttackTokenPool.h"

bool FCC_AttackTokenPool::TryAcquire(const FObjectKey& Key, int32 Cost)
{
	if (Holders.Contains(Key))
	{
		return true;
	}
	const int32 SafeCost = FMath::Max(1, Cost);
	if (Used() + SafeCost > Capacity)
	{
		return false;
	}
	Holders.Add(Key, SafeCost);
	return true;
}

bool FCC_AttackTokenPool::Release(const FObjectKey& Key)
{
	return Holders.Remove(Key) > 0;
}

bool FCC_AttackTokenPool::Holds(const FObjectKey& Key) const
{
	return Holders.Contains(Key);
}

int32 FCC_AttackTokenPool::Used() const
{
	int32 Total = 0;
	for (const TPair<FObjectKey, int32>& Pair : Holders)
	{
		Total += Pair.Value;
	}
	return Total;
}

int32 FCC_AttackTokenPool::Available() const
{
	return FMath::Max(0, Capacity - Used());
}

void FCC_AttackTokenPool::PruneIf(TFunctionRef<bool(const FObjectKey&)> ShouldRemove)
{
	for (auto It = Holders.CreateIterator(); It; ++It)
	{
		if (ShouldRemove(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void FCC_AttackTokenPool::Reset()
{
	Holders.Reset();
}
