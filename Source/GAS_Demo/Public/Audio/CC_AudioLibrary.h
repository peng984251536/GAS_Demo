#pragma once

#include "CoreMinimal.h"
#include "Audio/CC_AudioConfig.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_AudioLibrary.generated.h"

/** 无需角色、关卡或 World Context 的全局数据入口；调用应在游戏线程。 */
UCLASS()
class GAS_DEMO_API UCC_AudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="CC|Audio", meta=(DisplayName="Get Audio Config"))
	static UCC_AudioConfig* GetAudioConfig();

	/** 读取全局配置并随机取一个音效；空配置或空事件返回 nullptr，不会自动播放。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio", meta=(DisplayName="Pick Global Audio Sound"))
	static USoundBase* PickGlobalAudioSound(FName Event);
};
