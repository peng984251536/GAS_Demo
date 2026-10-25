#include "Audio/CC_AudioSettings.h"
#include "Audio/CC_AudioConfig.h"

UCC_AudioConfig* UCC_AudioSettings::GetAudioConfig()
{
	UCC_AudioSettings* Settings = GetMutableDefault<UCC_AudioSettings>();
	Settings->LoadedAudioConfig = Settings->DefaultAudioConfig.LoadSynchronous();
	return Settings->LoadedAudioConfig;
}
