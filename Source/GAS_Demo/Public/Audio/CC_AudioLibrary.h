#pragma once

#include "CoreMinimal.h"
#include "Audio/CC_AudioConfig.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_AudioLibrary.generated.h"

class UAudioComponent;
class USceneComponent;

/**
 * 音效的唯一播放入口：调用方只给"事件名 + 在哪播"，播哪个音效、音量音高怎么随机、
 * 衰减和并发怎么限制，全部由全局配置（UCC_AudioConfig）里该事件的规则决定。
 *
 *   UI / 全局：   PlayAudio2D(this, "UI.Click")
 *   世界中一点：  PlayAudioAtLocation(this, "Hit.Light", ImpactPoint)
 *   跟随物体：    PlayAudioAttached(this, "Attack.Swing", Character->GetRootComponent())
 *
 * 只在本机播放，不做网络同步；专用服务器上直接跳过。事件未配置时不播放，并在日志里警告一次。
 */
UCLASS()
class GAS_DEMO_API UCC_AudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 2D 播放（无空间位置），按 UI 音效处理：游戏暂停时也会播放。VolumeScale 叠乘在事件音量之上。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio", meta=(WorldContext="WorldContextObject"))
	static void PlayAudio2D(const UObject* WorldContextObject,
		UPARAM(meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames")) FName Event, float VolumeScale = 1.f);

	/** 在世界中的一个位置播放一次（命中点、落脚点等），使用事件的衰减与并发设置。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio", meta=(WorldContext="WorldContextObject"))
	static void PlayAudioAtLocation(const UObject* WorldContextObject,
		UPARAM(meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames")) FName Event, FVector Location, float VolumeScale = 1.f);

	/**
	 * 附着在组件上播放，声音跟随移动（挥砍、施法等）。返回音频组件，可用于提前停止；未播放时返回空。
	 * 组件销毁时声音继续播完。
	 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio")
	static UAudioComponent* PlayAudioAttached(
		UPARAM(meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames")) FName Event,
		USceneComponent* AttachTo, FName SocketName = NAME_None, float VolumeScale = 1.f);

	/** 全局配置（启动时已预加载）。 */
	UFUNCTION(BlueprintPure, Category="CC|Audio", meta=(DisplayName="Get Audio Config"))
	static UCC_AudioConfig* GetAudioConfig();

	/** 只取音效不播放（例如交给 CommonButton 的 Sound Override）；不做避免重复。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio", meta=(DisplayName="Pick Global Audio Sound"))
	static USoundBase* PickGlobalAudioSound(UPARAM(meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames")) FName Event);

	/** 配置里所有事件名，供编辑器下拉框使用。 */
	UFUNCTION()
	static TArray<FName> GetAudioEventNames();
};
