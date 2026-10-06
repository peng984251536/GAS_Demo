// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/CC_TargetingTypes.h"
#include "GameplayEffect.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_BlueprintLibrary.generated.h"

class ACC_EnemyCharacter;

UENUM(BlueprintType)
enum EHitDirection
{
	Forward0,
	Back,
	Left,
	Right,
};


/**
 * 
 */
UCLASS()
class GAS_DEMO_API UCC_BlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 在服务器/单机创建怪物并补建默认 AIController；生成失败返回 nullptr。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crash|Enemy",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "EnemyClass"))
	static ACC_EnemyCharacter* SpawnEnemy(const UObject* WorldContextObject,
		TSubclassOf<ACC_EnemyCharacter> EnemyClass, const FTransform& SpawnTransform);

	UFUNCTION(BlueprintPure)
	static EHitDirection GetHitDirection(const FVector& TargetForward,const FVector& ToInstigator);

	UFUNCTION(BlueprintPure)
	static FString GetHitDirectionName(const EHitDirection Direction);

	/**
	 * 寻找最近的目标
	 * @param WorldContextObject 
	 * @param Origin 
	 * @param Tag 
	 * @return 
	 */
	UFUNCTION(BlueprintCallable)
	static FClosestActorWithTagResult FindClosestActorWithTag(
		const UObject* WorldContextObject,
		const FVector& Origin,
		const FName& Tag
	);

	/**
	 * 寻找最近的目标
	 * @param WorldContextObject 
	 * @param Origin 
	 * @param Tag 
	 * @return 
	 */
	UFUNCTION(BlueprintCallable)
	static void ApplyGameplayEffectSpecByTag(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		const TSubclassOf<UGameplayEffect> GAEffect,
		//const FGameplayEffectSpecHandle& SpecHandle,
		const FGameplayTag& SetByCallTag,
		int Damage
	);

	UFUNCTION(BlueprintCallable)
	// 在服务器确认命中后调用。
	// SourceASC：攻击者的 ASC。
	// TargetASC：受击者的 ASC。
	void ApplyHeavyAttackDamage(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC);
	
};


USTRUCT(BlueprintType)
struct FGameplayTagRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString AttackInput = "CCTags.Event.AttackInput";
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DodgeInput = "CCTags.Event.DodgeInput";
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString AttackAction = "CCTags.Event.AttackAction";
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DodgeAction = "CCTags.Event.DodgeAction";
};

