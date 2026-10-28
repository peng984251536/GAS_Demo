// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Mechanics/CC_Dodge.h"
#include "GAS_Demo.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UCC_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}
	if (!IsValid(CombatComponent) || !IsValid(OwnedAction) || !IsValid(OwnedAction->Montage))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Dodge requires an action component and action montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 播放蒙太奇
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("PlayActionMontage"),
		OwnedAction->Montage,
		2.5f,
		NAME_None,
		true);	
	
	if (!IsValid(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 闪避期间不转向；EndAbility 里恢复（正常结束、打断、取消都会走到）。
	LockRotation(ActorInfo);

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();
	
	// 方式二：屏幕打印，调试时更直观
	if (GEngine && ShowDebug)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("Ability Activated: %s"), *GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("Ability Activated: %s"), *GetName()));
	}
	
}

void UCC_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RestoreRotation(ActorInfo);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCC_Dodge::LockRotation(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (bRotationLocked)
	{
		return;
	}
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement))
	{
		return;
	}
	SavedRotationRate = Movement->RotationRate;
	Movement->RotationRate = FRotator::ZeroRotator;
	bRotationLocked = true;
}

void UCC_Dodge::RestoreRotation(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bRotationLocked)
	{
		return;
	}
	bRotationLocked = false;
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->RotationRate = SavedRotationRate;
	}
}
