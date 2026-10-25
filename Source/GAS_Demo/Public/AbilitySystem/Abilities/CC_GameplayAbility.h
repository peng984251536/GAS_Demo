// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActionComponent.h"
#include "Data/AbilityData/CombatActionData.h"
#include "Data/AbilityData/CombatActionSet.h"
#include "Input/FBufferedAbilityInput.h"
#include "AbilitySystemComponent.h"
#include "CC_GameplayAbilityBase.h"
#include "GameplayTags/CC_Tags.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "CC_GameplayAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UActionComponent;
class UCombatActionData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_GameplayAbility : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:



	// Sets default values for this component's properties
	UCC_GameplayAbility();

	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	void SetAbilityLevel(int32 Level);
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	void AddAbilityLevel(int32 Level);

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Ability|", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Ability|", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatActionData> OwnedAction;

	// 蒙太奇播放完成
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	virtual void OnMontageCompleted();
	// 蒙太奇淡出
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	virtual void OnMontageBlendOut();
	// 蒙太奇被打断
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	virtual void OnMontageInterrupted();
	// 蒙太奇被取消
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	virtual void OnMontageCancelled();
	UFUNCTION(BlueprintCallable, Category="Ability|Abilities")
	virtual void FinishOwnedAction(bool bWasCancelled);

	/**
	 * 从全局音效配置（UCC_AudioConfig）随机取 Event 对应的音效，附着在 Avatar 上播放。
	 * 只在执行本能力的机器上播放，不做网络同步；Event 为空、未配置或专用服务器时跳过。
	 */
	void PlayAudioEventOnAvatar(FName Event) const;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                         const FGameplayAbilityActivationInfo ActivationInfo,
	                         FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	                         const FGameplayEventData* TriggerEventData = nullptr) override;
};
