// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "Engine/HitResult.h"
#include "CC_Primary.generated.h"


class UCC_AbilitySystemComponent;

USTRUCT(BlueprintType)
struct GAS_DEMO_API FHitResultAndActor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FHitResult Result;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	AActor* hitActor = nullptr;
};

/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_Primary : public UCC_GameplayAbility
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,Category = "Crash|Abilities")
	TArray<FHitResultAndActor> HitBoxOverlapText();
	//UFUNCTION(BlueprintCallable,Category = "Crash|Abilities")
	void DrawHitBoxOverlapDebugs(const TArray<FHitResultAndActor>& Hits,const FVector& HitBoxLocation) const;
	UFUNCTION(BlueprintCallable,Category = "Crash|Abilities")
	void SendEventToActor(const TArray<FHitResultAndActor>& Hits);

	/** 激活：数据驱动选招（或读显式 payload）→ ASC 门控+提交 → 经注册表派发对应机制 GA。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
private:
	int32 OwnedGeneration = -1;

	

	
	UPROPERTY(EditDefaultsOnly,Category="Crash|Abilities")
	float HitBoxRadius = 100.0f;
	UPROPERTY(EditDefaultsOnly,Category="Crash|Abilities")
	float HitBoxForwardOffset = 200.0f;
	UPROPERTY(EditDefaultsOnly,Category="Crash|Abilities")
	float HitBoxElevationOffset = 20.0f;

	
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities|Attack")
	void SetAttackFacingFromAvatarInput(
		float InterpSpeed = 18.0f);
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities|Combo")
	void SetAttackFacing(FVector InputDirection,float InterpSpeed);
};


