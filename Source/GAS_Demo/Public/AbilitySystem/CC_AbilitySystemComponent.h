// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Utils/FBufferedAbilityInput.h"
#include "CC_AbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCC_AbilitySystemComponent();

	void HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec );

	UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	void SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass,int32 Level);
	UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	void AddAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass,int32 Level);

	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool HandleComboInput(
	// const FGameplayTag& AttackAbilityTag);
	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool CancelAndActivate(
	// 	const FGameplayTag& CancelAbilityTag,
	// 	const FGameplayTag& NewAbilityTag);
	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool HandleQueuedInput(
	// 	const FGameplayTag& WaitForAbilityTag,
	// 	const FGameplayTag& NewAbilityTag);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// bool ConsumeComboInput(
	// 	const FGameplayTag& AttackAbilityTag,
	// 	float MaxBufferTime = 0.25f);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void ClearBufferedInput();

protected:
	UPROPERTY(EditDefaultsOnly, Category="Ability|Input")
	float DefaultBufferTime = 0.25f;
	
	FBufferedAbilityInput BufferedInput;
	
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRep_ActivateAbilities() override;
	virtual FActiveGameplayEffectHandle ApplyEffectSpecToSelf(FGameplayTag& gameplayTag,UClass* gameplayEffect);

	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void HandleAbilityEnded(UGameplayAbility* EndedAbility);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void CacheInput(
	// 	const FGameplayTag& AbilityTag,
	// 	const FGameplayTag& WaitForAbilityTag,
	// 	ECCAbilityLinkType LinkType);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// bool IsBufferedInputFresh(float MaxBufferTime) const;
	UFUNCTION(BlueprintCallable, Category="Ability|Input")
	bool IsAbilityWithTagActive(const FGameplayTag& AbilityTag) const;
	

};
