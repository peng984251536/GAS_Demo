#pragma once

#include "CommonButtonBase.h"
#include "CC_ButtonBase.generated.h"

/**
 * 项目通用按钮基类：初始化时从全局音效配置（UCC_AudioConfig）读取悬停 / 点击音效，
 * 写入 CommonButton 官方的 Hovered / Pressed Sound Override。
 * 事件设为 None 时不覆盖，保留蓝图里手动设置的 Override 或按钮样式（CommonButtonStyle）中的音效。
 * 每个按钮在初始化时选定一个变体；需要每次随机时，在配置里使用带随机节点的 Sound Cue。
 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_ButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	/** 悬停音效事件名，默认 UI.Hover。 */
	UPROPERTY(EditDefaultsOnly, Category="Sound", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName HoverSoundEvent = TEXT("UI.Hover");

	/** 点击音效事件名，默认 UI.Click；确认类按钮可改为 UI.Confirm。 */
	UPROPERTY(EditDefaultsOnly, Category="Sound", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName ClickSoundEvent = TEXT("UI.Click");
};
