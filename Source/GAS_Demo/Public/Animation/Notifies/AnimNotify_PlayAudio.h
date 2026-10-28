#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayAudio.generated.h"

/**
 * 通用音效 Notify：放在攻击、受击、施法、落地等动画的关键帧，按事件名从全局音效配置播放。
 * 音效变体、音量音高随机、衰减、并发都由配置里该事件的规则决定，这里只决定"哪一帧、在哪响"。
 *
 * 动画（含蒙太奇）在每台机器上都会播放，所以每个客户端都能听到，不需要网络同步；专用服务器跳过。
 * Notify 是共享资产，运行时不保存任何状态。
 */
UCLASS(meta=(DisplayName="CC Play Audio"))
class GAS_DEMO_API UAnimNotify_PlayAudio : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 音效事件名，如 Attack.Swing、Hit.Light。 */
	UPROPERTY(EditAnywhere, Category="Audio", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName Event;

	/** 勾选：附着在网格体上，声音跟随角色移动（挥砍、施法）；不勾：在当前位置播一次后原地不动（落地、爆点）。 */
	UPROPERTY(EditAnywhere, Category="Audio")
	bool bFollowMesh = true;

	/** 声音的位置：骨骼或插槽名，如 hand_r、weapon_tip；为空使用网格体原点。 */
	UPROPERTY(EditAnywhere, Category="Audio")
	FName SocketName;

	/** 叠乘在事件音量之上，用于单个动画微调。 */
	UPROPERTY(EditAnywhere, Category="Audio", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.f;
};
