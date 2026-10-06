// Fill out your copyright notice in the Description page of Project Settings.


#include "Notifies/ANS_Comboing.h"
#include "GAS_Demo.h"

#include "Character/CC_BaseCharacter.h"

void UANS_Comboing::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	// 编辑蒙太奇时，预览 Mesh 的 Owner 是编辑器预览 Actor，不是游戏里的 ACC_BaseCharacter。
	// 预览不需要修改角色朝向，直接跳过，避免把正常预览误报成能力激活失败。
	const UWorld* World = MeshComp->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}
	
	if (!IsValid(MeshComp))
	{
		if(GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UCombatActionComponent::OpenCombo nullptr"));
			GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
				FString::Printf(TEXT("UCombatActionComponent::OpenCombo nullptr")));
		}
		return;
	}
	const ACC_BaseCharacter* BaseCharacter = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
	if(!IsValid(BaseCharacter))
	{
		if (GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UCombatActionComponent::ACC_BaseCharacter nullptr: %s"), *GetName());
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
				FString::Printf(TEXT("UCombatActionComponent::ACC_BaseCharacter nullptr: %s"), *GetName()));
		}
		return;
	}

	
}

void UANS_Comboing::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
