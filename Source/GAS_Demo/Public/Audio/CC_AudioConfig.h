#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CC_AudioConfig.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_AudioEntry
{
	GENERATED_BODY()

	/** Sound Wave 或 Sound Cue；空项会在随机选择时跳过。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio")
	TArray<TObjectPtr<USoundBase>> Sounds;
};

/** 只读共享数据。全局默认资产由项目设置 → Game → CC Audio 指定。 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_AudioConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TMap<FName, FCC_AudioEntry> Events;

	/** 如 UI.Click、Hit.Heavy。随机取一个有效变体；未配置时返回空，不负责播放。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio")
	USoundBase* PickSound(FName Event) const;
};
