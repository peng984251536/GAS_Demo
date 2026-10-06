// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "GAS_Demo.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/CC_BaseCharacter.h"
#include "Data/AbilityData/CombatActionData.h"

UCC_GameplayAbility::UCC_GameplayAbility()
{
}

#pragma region AbilityLevel

void UCC_GameplayAbility::SetAbilityLevel(int32 Level)
{
	if (!IsValid(GetAbilitySystemComponentFromActorInfo()))
		return;

	if (UCC_AbilitySystemComponent* ASC = Cast<UCC_AbilitySystemComponent>
		(GetAbilitySystemComponentFromActorInfo()))
	{
		ASC->SetAbilityLevel(GetClass(), Level);
	}
}

void UCC_GameplayAbility::AddAbilityLevel(int32 Level)
{
	if (!IsValid(GetAbilitySystemComponentFromActorInfo()))
		return;

	if (UCC_AbilitySystemComponent* ASC = Cast<UCC_AbilitySystemComponent>
		(GetAbilitySystemComponentFromActorInfo()))
	{
		ASC->AddAbilityLevel(GetClass(), Level);
	}
}


#pragma endregion  等级


/**
 * 激活能力
 * @param Handle 
 * @param ActorInfo 
 * @param ActivationInfo 
 * @param TriggerEventData 
 */
void UCC_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	// GAS 即使在 PreActivate 中结束能力，也仍会调用 ActivateAbility。
	if (!IsActive())
	{
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UCC_GameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo,
                                      const FGameplayAbilityActivationInfo ActivationInfo,
                                      FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
                                      const FGameplayEventData* TriggerEventData)
{
	// 先建立激活状态和角色/组件缓存，再检查 IsActive 和解析动作数据。
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	OwnedAction = nullptr;
	if (!IsActive())
	{
		return;
	}

	OwnedAction = TriggerEventData
		              ? Cast<UCombatActionData>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()))
		              : nullptr;
	if (!IsValid(OwnedAction))
	{
		// 血量监听和攻击意图不需要动作数据；具体蒙太奇能力在使用前验证。
		return;
	}

	// 判断是否可执行
	if (!IsValid(OwnedAction->Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		// 方式二：屏幕打印，调试时更直观
		if (GEngine && ShowDebug)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("ActivateAbility->CombatComponent:Montage nullptr: %s"), *GetName());
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
			                                 FString::Printf(
				                                 TEXT("ActivateAbility->CombatComponent:Montage nullptr: %s"),
				                                 *GetName()));
		}
		return;
	}

}


#pragma region Montage
void UCC_GameplayAbility::OnMontageCompleted()
{
	
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[Ability][Montage] Completed: Ability=%s Instance=%s Avatar=%s Action=%s Montage=%s Active=%d Task=%s"),
		*GetNameSafe(GetClass()), *GetNameSafe(this), *GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(OwnedAction), IsValid(OwnedAction) ? *GetNameSafe(OwnedAction->Montage) : TEXT("None"),
		IsActive(), *GetNameSafe(MontageTask));
	FinishOwnedAction(false);
}

void UCC_GameplayAbility::OnMontageBlendOut()
{
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[Ability][Montage] BlendOut: Ability=%s Instance=%s Avatar=%s Action=%s Montage=%s Active=%d Task=%s"),
		*GetNameSafe(GetClass()), *GetNameSafe(this), *GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(OwnedAction), IsValid(OwnedAction) ? *GetNameSafe(OwnedAction->Montage) : TEXT("None"),
		IsActive(), *GetNameSafe(MontageTask));
	// 这里不结束。OnCompleted 会随后结束；
	// 保持 Ability 生命周期完整，避免混出帧提前终止。
}

void UCC_GameplayAbility::OnMontageInterrupted()
{
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[Ability][Montage] Interrupted: Ability=%s Instance=%s Avatar=%s Action=%s Montage=%s Active=%d Task=%s"),
		*GetNameSafe(GetClass()), *GetNameSafe(this), *GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(OwnedAction), IsValid(OwnedAction) ? *GetNameSafe(OwnedAction->Montage) : TEXT("None"),
		IsActive(), *GetNameSafe(MontageTask));
	FinishOwnedAction(true);
}

void UCC_GameplayAbility::OnMontageCancelled()
{
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[Ability][Montage] Cancelled: Ability=%s Instance=%s Avatar=%s Action=%s Montage=%s Active=%d Task=%s"),
		*GetNameSafe(GetClass()), *GetNameSafe(this), *GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(OwnedAction), IsValid(OwnedAction) ? *GetNameSafe(OwnedAction->Montage) : TEXT("None"),
		IsActive(), *GetNameSafe(MontageTask));
	FinishOwnedAction(true);
}

#pragma endregion

void UCC_GameplayAbility::FinishOwnedAction(bool bWasCancelled)
{
	if (IsValid(CombatComponent))
	{
		CombatComponent->EndActionIfOwned(OwnedAction);
	}

	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		bWasCancelled);
}
