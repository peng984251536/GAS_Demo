// Fill out your copyright notice in the Description page of Project Settings.


#include "Notifies/ANS_ComboStart.h"
#include "GAS_Demo.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UANS_ComboStart::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}
	
	// 编辑蒙太奇时，预览 Mesh 的 Owner 是编辑器预览 Actor，不是游戏里的 ACC_BaseCharacter。
	// 预览不需要修改角色朝向，直接跳过，避免把正常预览误报成能力激活失败。
	const UWorld* World = MeshComp->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}
	ACC_BaseCharacter* BaseCharacter = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
	if(!IsValid(BaseCharacter))
	{
		// 真正游戏中若仍出现这种情况，记录 Mesh 和 Owner，方便查是哪只角色绑定错了通知。
		UE_LOG(LogGAS_Demo, Warning, TEXT("ANS_ComboStart: Mesh %s has unexpected owner %s (expected ACC_BaseCharacter)."),
			*GetNameSafe(MeshComp), *GetNameSafe(MeshComp->GetOwner()));
		return;
	}

	SetAttackFacing(BaseCharacter,BaseCharacter->GetLastMoveInputDirection());
}

void UANS_ComboStart::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UANS_ComboStart::SetAttackFacing(ACC_BaseCharacter* BaseCharacter,FVector InputDirection,float InterpSpeed)
{

	// 只处理地面朝向，忽略 Z。
	FVector FlatDirection = InputDirection;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		return;
	}

	FlatDirection.Normalize();
	
	const FRotator TargetRotation =
		FRotator(0.0f, FlatDirection.Rotation().Yaw, 0.0f);

	const float DeltaSeconds = GetWorld()
		? GetWorld()->GetDeltaSeconds()
		: 0.0f;

	const FRotator NewRotation =
		InterpSpeed > 0.0f && DeltaSeconds > 0.0f
		? FMath::RInterpTo(
			BaseCharacter->GetActorRotation(),
			TargetRotation,
			DeltaSeconds,
			InterpSpeed)
		: TargetRotation;

	UE_LOG(LogGAS_Demo, Warning,
		TEXT("[BowAI][Facing] Store Character=%s Raw=%s Stored=%s"),
		*GetNameSafe(this),
		*FlatDirection.ToCompactString(),
		*NewRotation.ToCompactString());
	BaseCharacter->SetActorRotation(NewRotation);
}


