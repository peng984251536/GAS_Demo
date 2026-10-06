// Fill out your copyright notice in the Description page of Project Settings.


#include "Notifies/ANS_ComboWindow.h"
#include "GAS_Demo.h"

#include "Character/CC_BaseCharacter.h"
#include "Components/ActionComponent.h"

void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                   const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		if(GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UActionComponent::OpenCombo nullptr"));
			GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
				FString::Printf(TEXT("UActionComponent::OpenCombo nullptr")));
		}
		return;
	}
	const ACC_BaseCharacter* BaseCharacter = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
	if(!IsValid(BaseCharacter))
	{
		// if (GEngine)
		// {
		// 	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
		// 		FString::Printf(TEXT("UActionComponent::OpenCombo nullptr: %s"), *GetName()));
		// }
		return;
	}
	UActionComponent* Combat = BaseCharacter->GetUCombatActionComponent();
	if (!IsValid(Combat))
	{
		if(GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UActionComponent::OpenCombo nullptr"));
			GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
				FString::Printf(TEXT("UActionComponent::OpenCombo nullptr")));
		}
		return;
	}

	UE_LOG(LogGAS_Demo, Log, TEXT("UANS_ComboWindow::NotifyEnd OpenComboWindowForMontage: %s"), *GetName());
	Combat->OpenComboWindowForMontage(Animation);
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (UActionComponent* Combat = Owner->FindComponentByClass<UActionComponent>())
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("UANS_ComboWindow::NotifyEnd CloseComboWindowForMontage: %s"), *GetName());
		Combat->CloseComboWindowForMontage(Animation);
	}
}
