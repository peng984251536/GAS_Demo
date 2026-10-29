#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Math/Interval.h"
#include "Chaos/ChaosEngineInterface.h"
#include "CC_AudioConfig.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

/**
 * 一个音效事件：可选的音效变体 + 播放规则。
 * 调用方只给事件名，音量 / 音高随机、衰减、并发、避免重复都在这里配置。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_AudioEntry
{
	GENERATED_BODY()

	/** Sound Wave 或 Sound Cue；空项会在随机选择时跳过。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** 每次播放在此范围内随机音量倍率。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Variation")
	FFloatInterval VolumeRange = FFloatInterval(1.f, 1.f);

	/** 每次播放在此范围内随机音高倍率，例如脚步填 0.95 ~ 1.05。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Variation")
	FFloatInterval PitchRange = FFloatInterval(1.f, 1.f);

	/** 有多个变体时，不连续两次播放同一个。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Variation")
	bool bAvoidRepeat = true;

	/** 3D 播放（指定位置 / 附着）时的衰减；为空使用配置的 DefaultAttenuation。2D 播放忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Playback")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** 并发限制，例如脚步同时最多 8 个；为空时使用音效资产自身的设置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Playback")
	TObjectPtr<USoundConcurrency> Concurrency;
};

/** 只读共享数据。全局默认资产由项目设置 → Game → CC Audio 指定；运行时状态放在 UCC_AudioSubsystem。 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_AudioConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TMap<FName, FCC_AudioEntry> Events;

	/** 事件未单独配置衰减时，3D 播放使用的默认衰减。为空则 3D 音效不随距离衰减。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TObjectPtr<USoundAttenuation> DefaultAttenuation;

	/** 查找事件配置；未配置返回 nullptr。 */
	const FCC_AudioEntry* FindEntry(FName Event) const { return Events.Find(Event); }

	/** 如 UI.Click、Hit.Heavy。随机取一个有效变体（不记录历史，不做避免重复）；未配置时返回空，不负责播放。 */
	UFUNCTION(BlueprintCallable, Category="CC|Audio")
	USoundBase* PickSound(FName Event) const;

	/**
	 * 地面物理表面类型 → 脚步事件名，所有脚步 Notify 共用这一张表。
	 * 表面类型先在 项目设置 → Physics → Physical Surface 命名，例如 SurfaceType1 = Grass → Footstep.Grass。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, FName> FootstepSurfaceEvents;

	/** 没检测到地面，或表面类型不在上表中时使用的脚步事件。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Footstep")
	FName DefaultFootstepEvent = TEXT("Footstep.Concrete");

	/** 按表面类型查脚步事件名；未配置时返回 DefaultFootstepEvent。 */
	UFUNCTION(BlueprintPure, Category="CC|Audio")
	FName GetFootstepEvent(TEnumAsByte<EPhysicalSurface> Surface) const;
};
