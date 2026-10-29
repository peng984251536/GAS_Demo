#include "AI/Squad/CC_SquadTypes.h"

bool FCC_SquadRules::PassesFollowLeash(const FCC_SquadOrder& Order, const FVector& AnchorLocation,
	const FVector& SelfLocation, const FVector& CandidateLocation, bool bIsCurrentTarget)
{
	if (FVector::Dist2D(SelfLocation, AnchorLocation) > Order.ReturnRadius)
	{
		return false;
	}
	const float Limit = bIsCurrentTarget ? FMath::Max(Order.LeashRadius, Order.EngageRadius) : Order.EngageRadius;
	return FVector::Dist2D(CandidateLocation, AnchorLocation) <= Limit;
}
