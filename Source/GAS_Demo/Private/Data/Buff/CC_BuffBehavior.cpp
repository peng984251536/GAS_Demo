#include "Data/Buff/CC_BuffBehavior.h"

UWorld* UCC_BuffBehavior::GetWorld() const
{
    return !HasAnyFlags(RF_ClassDefaultObject) && GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
