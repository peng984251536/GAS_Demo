#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Chaos/ChaosEngineInterface.h"
#include "AnimNotify_Footstep.generated.h"

/**
 * 放在走 / 跑动画的落脚帧：从脚部骨骼向下检测地面，按物理材质的表面类型选择脚步音效事件，
 * 再从全局音效配置（UCC_AudioConfig）随机取一个变体在落脚点播放。
 * 动画在每台机器上都会播放，所以不需要网络同步；专用服务器跳过。
 * Notify 是共享资产，运行时不保存任何状态。
 */
UCLASS(meta=(DisplayName="CC Footstep"))
class GAS_DEMO_API UAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 落脚的骨骼或插槽，例如 foot_l / foot_r；为空或找不到时用网格体原点。 */
	UPROPERTY(EditAnywhere, Category="Footstep")
	FName FootBone = TEXT("foot_l");

	/** 没检测到地面，或地面表面类型不在下表中时使用的事件。 */
	UPROPERTY(EditAnywhere, Category="Footstep")
	FName DefaultEvent = TEXT("Footstep.Concrete");

	/**
	 * 物理材质表面类型 → 脚步事件，例如 SurfaceType1 → Footstep.Grass。
	 * 表面类型在 项目设置 → Physics → Physical Surface 中命名，再给地面材质的物理材质选择对应类型。
	 */
	UPROPERTY(EditAnywhere, Category="Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, FName> SurfaceEvents;

	/** 从脚部往下检测地面的距离（厘米）。 */
	UPROPERTY(EditAnywhere, Category="Footstep", meta=(ClampMin="1.0"))
	float TraceDistance = 60.f;

	UPROPERTY(EditAnywhere, Category="Footstep", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.f;

	/** 每次在 1 ± 此值的范围内随机音高，减少重复感；0 表示不变。 */
	UPROPERTY(EditAnywhere, Category="Footstep", meta=(ClampMin="0.0", ClampMax="0.5"))
	float PitchVariation = 0.05f;
};
