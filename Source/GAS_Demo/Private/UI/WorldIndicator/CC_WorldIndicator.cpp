#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "GameFramework/Actor.h"

bool UCC_WorldIndicator::GetAnchorLocation(FVector& OutLocation) const
{
	if (AnchorActor.IsExplicitlyNull())
	{
		OutLocation = WorldLocation + WorldOffset;
		return true;
	}
	const AActor* Actor = AnchorActor.Get();
	if (!Actor || Actor->IsHidden()) return false;
	OutLocation = Actor->GetActorLocation() + WorldOffset;
	return true;
}
