// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"

#include "GAS_Demo.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/ActionComponent.h"

void UCC_GameplayAbilityBase::PreActivate(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	// GAS 在父类 PreActivate 中建立激活状态，不能放到 IsActive 检查之后。
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	BaseCharacter = nullptr;
	CombatComponent = nullptr;
	if (!IsActive())
	{
		return;
	}

	BaseCharacter = Cast<ACC_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(BaseCharacter))
	{
		if (GEngine && ShowDebug)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UCC_GameplayAbility::ActivateAbility->ACC_BaseCharacter nullptr: %s"),
				   *GetName());
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
											 FString::Printf(
												 TEXT(
													 "UCC_GameplayAbility::ActivateAbility->ACC_BaseCharacter nullptr: %s"),
												 *GetName()));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CombatComponent = BaseCharacter->GetUCombatActionComponent();
	// 纯查找能力可以没有战斗组件；使用组件的能力必须在使用前验证。
	if (!IsValid(CombatComponent))
	{
		if (GEngine && ShowDebug)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("ActivateAbility->CombatComponent nullptr: %s"), *GetName());
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
											 FString::Printf(
												 TEXT("ActivateAbility->CombatComponent nullptr: %s"), *GetName()));
		}
		return;
	}

}
