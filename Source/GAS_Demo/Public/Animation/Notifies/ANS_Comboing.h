// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_Comboing.generated.h"

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UANS_Comboing : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/**
	 * 放进 Montage 的攻击霸体时间段。
	 * 服务器 Begin 增加 SuperArmor，End（包括蒙太奇中断）移除本次贡献；不影响伤害。
	 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
