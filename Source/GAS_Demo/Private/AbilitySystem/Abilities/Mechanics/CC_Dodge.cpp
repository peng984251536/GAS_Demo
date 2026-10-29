// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Mechanics/CC_Dodge.h"
#include "GAS_Demo.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/CC_Tags.h"

UCC_Dodge::UCC_Dodge()
{
	// 每次闪避使用新实例：蓝图 BP_GA_PlayerDodge 的 IsCanMove 等变量依赖"每次激活从默认值开始"。
	// 改成 InstancedPerActor 会复用实例，上一次闪避留下的 IsCanMove = true 会让下一次闪避被移动输入立即打断。
	// 实例可能重叠，转向锁因此按计数实现，见 LockRotation / RestoreRotation。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
}

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
	if (GEngine && ShouldShowDebug())
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
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(ASC) || !IsValid(Movement))
	{
		return;
	}
	ASC->AddLooseGameplayTag(CCTags::Status::RotationLocked);
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
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(CCTags::Status::RotationLocked);
		if (ASC->GetTagCount(CCTags::Status::RotationLocked) > 0)
		{
			// 还有其他闪避实例持有转向锁，由最后一个结束的实例恢复。
			return;
		}
	}
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement))
	{
		return;
	}
	// 恢复为移动组件的默认配置（角色蓝图 / 构造函数里设置的转向速度）。
	if (const UCharacterMovementComponent* Defaults = Cast<UCharacterMovementComponent>(Movement->GetArchetype()))
	{
		Movement->RotationRate = Defaults->RotationRate;
	}
}
