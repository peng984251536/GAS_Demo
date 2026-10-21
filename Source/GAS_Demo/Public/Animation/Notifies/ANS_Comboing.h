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
	 * 放进 Montage 的可连招时间段。
	 * Begin 打开输入窗口，End 关闭输入窗口。
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
