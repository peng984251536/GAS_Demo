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

	/**
	 * 首次同步加载并加入 Root Set，供后续全局访问。未配置或加载失败时返回 nullptr。
	 * 注意：不能把加载结果存到本类成员上。打包版本里 CDO 位于 "Disregard for GC" 池，
	 * CDO 引用普通资产会在加载地图时触发 VerifyGCAssumptions 致命错误。
	 */
	static UCC_AudioConfig* GetAudioConfig();
};
