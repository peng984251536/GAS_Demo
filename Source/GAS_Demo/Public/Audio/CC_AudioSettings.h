#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CC_AudioSettings.generated.h"

class UCC_AudioConfig;

/** 项目设置 → Game → CC Audio；各地图、角色与 UI 共用同一份默认音效配置。 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="CC Audio"))
class GAS_DEMO_API UCC_AudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<UCC_AudioConfig> DefaultAudioConfig;

	/** 首次同步加载；保留强引用以供后续全局访问。未配置或加载失败时返回 nullptr。 */
	static UCC_AudioConfig* GetAudioConfig();

private:
	UPROPERTY(Transient)
	TObjectPtr<UCC_AudioConfig> LoadedAudioConfig;
};
