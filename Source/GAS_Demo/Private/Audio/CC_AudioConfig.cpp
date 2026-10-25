#include "Audio/CC_AudioConfig.h"
#include "Sound/SoundBase.h"

USoundBase* UCC_AudioConfig::PickSound(FName Event) const
{
	const FCC_AudioEntry* Entry = Events.Find(Event);
	if (!Entry)
	{
		return nullptr;
	}

	TArray<USoundBase*> Variants;
	for (USoundBase* Sound : Entry->Sounds)
	{
		if (IsValid(Sound))
		{
			Variants.Add(Sound);
		}
	}
	return Variants.IsEmpty() ? nullptr : Variants[FMath::RandHelper(Variants.Num())];
}
