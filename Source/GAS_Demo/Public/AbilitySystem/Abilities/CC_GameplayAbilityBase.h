// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "CC_GameplayAbilityBase.generated.h"

class ACC_BaseCharacter;
class UActionComponent;

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_GameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// 输入配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool ShowDebug = true;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Ability|", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACC_BaseCharacter> BaseCharacter;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Ability|", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActionComponent> CombatComponent;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
						 const FGameplayAbilityActivationInfo ActivationInfo,
						 FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
						 const FGameplayEventData* TriggerEventData = nullptr) override;
};
