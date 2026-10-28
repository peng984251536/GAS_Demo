#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CC_AudioSubsystem.generated.h"

class UCC_AudioConfig;
class USoundBase;
struct FCC_AudioEntry;

/** 一次取音结果：选中的音效、这次的音量 / 音高，以及事件的播放规则。 */
struct FCC_AudioPick
{
	USoundBase* Sound = nullptr;
	const FCC_AudioEntry* Entry = nullptr;
	float Volume = 1.f;
	float Pitch = 1.f;
};

/**
 * 音效运行时：启动时预加载全局配置，按事件规则选出这一次要播的音效。
 * 共享配置只读，"上次播放了哪个变体"这类状态只保存在这里（每个 GameInstance 一份，不做网络同步）。
 * 一般不直接使用，播放请调用 UCC_AudioLibrary 的 PlayAudio2D / PlayAudioAtLocation / PlayAudioAttached。
 */
UCLASS()
class GAS_DEMO_API UCC_AudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 专用服务器不播放声音，也不创建本子系统。 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	/** 预加载全局配置及其引用的音效，避免第一次播放时卡顿。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 按事件规则取音：随机变体（可避免连续重复）+ 随机音量 / 音高。未配置时 Sound 为空，并只警告一次。 */
	FCC_AudioPick Pick(FName Event);

	/** 从任意 World Context 找到本子系统；编辑器动画预览等没有 GameInstance 的场合返回空。 */
	static UCC_AudioSubsystem* Get(const UObject* WorldContextObject);

	/** 不依赖子系统的取音（无避免重复），供动画预览等场合兜底。 */
	static FCC_AudioPick PickStateless(const UCC_AudioConfig* Config, FName Event);

private:
	UPROPERTY(Transient)
	TObjectPtr<UCC_AudioConfig> Config;

	/** 事件 → 上次播放的变体下标，用于避免连续重复。 */
	TMap<FName, int32> LastVariant;
	/** 已经警告过"未配置"的事件，避免刷屏。 */
	TSet<FName> WarnedEvents;
};
