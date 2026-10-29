#include "UI/Framework/CC_ButtonBase.h"
#include "Audio/CC_AudioLibrary.h"
#include "Sound/SoundBase.h"

// 只在事件名有效且配置里取得到音效时覆盖，取不到时保持按钮原有设置。
void UCC_ButtonBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!HoverSoundEvent.IsNone())
	{
		if (USoundBase* Sound = UCC_AudioLibrary::PickGlobalAudioSound(HoverSoundEvent))
		{
			SetHoveredSoundOverride(Sound);
		}
	}
	if (!ClickSoundEvent.IsNone())
	{
		if (USoundBase* Sound = UCC_AudioLibrary::PickGlobalAudioSound(ClickSoundEvent))
		{
			SetPressedSoundOverride(Sound);
		}
	}
}
