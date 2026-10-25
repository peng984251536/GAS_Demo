#include "Audio/CC_AudioLibrary.h"
#include "Audio/CC_AudioSettings.h"

UCC_AudioConfig* UCC_AudioLibrary::GetAudioConfig()
{
	return UCC_AudioSettings::GetAudioConfig();
}

USoundBase* UCC_AudioLibrary::PickGlobalAudioSound(FName Event)
{
	const UCC_AudioConfig* Config = GetAudioConfig();
	return Config ? Config->PickSound(Event) : nullptr;
}
