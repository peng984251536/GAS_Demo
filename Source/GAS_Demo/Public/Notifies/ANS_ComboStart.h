// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_ComboStart.generated.h"

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UANS_ComboStart : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 游戏中在通知开始时将角色朝向转向最近记录的输入方向；编辑器预览不执行。 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	UFUNCTION(BlueprintCallable, Category="Crash|Abilities|Combo")
	void SetAttackFacing(ACC_BaseCharacter* BaseCharacter,FVector InputDirection,float InterpSpeed = 45.0f);
};
