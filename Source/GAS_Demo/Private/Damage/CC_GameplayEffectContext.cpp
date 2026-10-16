#include "Damage/CC_GameplayEffectContext.h"

const FCC_GameplayEffectContext* FCC_GameplayEffectContext::Get(const FGameplayEffectContextHandle& Handle)
{
	return Handle.Get() && Handle.Get()->GetScriptStruct()->IsChildOf(StaticStruct())
		       ? static_cast<const FCC_GameplayEffectContext*>(Handle.Get())
		       : nullptr;
}

FCC_GameplayEffectContext* FCC_GameplayEffectContext::GetMutable(FGameplayEffectContextHandle& Handle)
{
	return const_cast<FCC_GameplayEffectContext*>(Get(Handle));
}

bool FCC_GameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	const bool bMapped = FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);
	uint8 Critical = bCritical ? 1 : 0;
	Ar.SerializeBits(&Critical, 1);
	if (Ar.IsLoading()) bCritical = Critical != 0;
	bOutSuccess = bOutSuccess && !Ar.IsError();
	return bMapped;
}
