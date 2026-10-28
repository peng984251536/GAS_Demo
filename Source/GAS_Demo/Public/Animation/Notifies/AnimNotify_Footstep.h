#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Footstep.generated.h"

/**
 * 放在走 / 跑动画的落脚帧：从脚部骨骼向下检测地面，读取物理材质的表面类型，
 * 到全局音效配置（UCC_AudioConfig::FootstepSurfaceEvents）查出脚步事件，随机取一个变体在落脚点播放。
 * 表面 → 事件的映射只在全局配置里维护，Notify 本身不保存映射。
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

	/**
	 * 可选：强制使用的事件名，忽略地面材质（例如落地动画填 Movement.Land）。
	 * 留空（None）时按地面表面类型查全局配置。
	 */
	UPROPERTY(EditAnywhere, Category="Footstep", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName EventOverride;

	/** 从脚部往下检测地面的距离（厘米）。 */
	UPROPERTY(EditAnywhere, Category="Footstep", meta=(ClampMin="1.0"))
	float TraceDistance = 60.f;

	/** 叠乘在事件音量之上，例如潜行动画调小。音高随机、衰减、并发在音效配置的事件里设置。 */
	UPROPERTY(EditAnywhere, Category="Footstep", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.f;
};
