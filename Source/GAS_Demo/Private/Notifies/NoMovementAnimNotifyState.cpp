#include "Notifies/NoMovementAnimNotifyState.h"

#include "Character/CC_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

void UNoMovementAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
	if (!IsValid(Character))
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(true);
	}

	if (UCharacterMovementComponent* Movement =
		Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void UNoMovementAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
	if (!IsValid(Character))
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(false);
	}
}