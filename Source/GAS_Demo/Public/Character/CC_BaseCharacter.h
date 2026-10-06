// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Data/CC_TargetingTypes.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"

#include "CC_BaseCharacter.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCC_AbilitySystemComponent;
class UCC_CharacterConfig;
class UCC_CharacterRuntimeData;
class UCC_AttributeSet;
class UActionComponent;

// 初始化属性后调用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FASCInitialized,
	UAbilitySystemComponent*, ASC,
	UAttributeSet*, AS
);
namespace CrashTags
{
	extern GAS_DEMO_API const FName Player;
	extern GAS_DEMO_API const FName Follower;
}


UCLASS(Abstract)
class GAS_DEMO_API ACC_BaseCharacter : public ACharacter,public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FASCInitialized OnASCInitialized;
	
	ACC_BaseCharacter();
	
	//UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 默认从角色蓝图读取；子类可覆盖配置来源。返回只读配置，不存放角色实例状态。 */
	UFUNCTION(BlueprintPure, Category = "Crash|Data")
	virtual const UCC_CharacterConfig* GetCharacterConfig() const;

	/** 此对象属于当前角色；角色之间不共享。 */
	UFUNCTION(BlueprintPure, Category = "Crash|Data")
	UCC_CharacterRuntimeData* GetRuntimeData() const;

	// 保留角色查询接口，让现有蓝图、AI、能力继续使用。
	UFUNCTION(BlueprintCallable, Category = "Crash|Abilities")
	void SetLastMoveInputDirection(const FVector& InDirection);
	UFUNCTION(BlueprintPure, Category = "Crash|Abilities")
	FVector GetLastMoveInputDirection() const;
	
	UFUNCTION(BlueprintPure, Category = "Crash|Abilities")
	bool IsAlive() const;
	UFUNCTION(BlueprintPure, Category = "Crash|Abilities")
	bool IsHit() const;
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual void HandleDeath();
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual void HandleRespawn();
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual void HandleHit();
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual void HandleRefreshHit();

	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crash|Enemy")
	virtual void SetClosestActor(const FClosestActorWithTagResult& InTarget);
	UFUNCTION(BlueprintPure, Category = "Crash|Enemy")
	virtual FClosestActorWithTagResult GetClosestActor() const;

	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual UCC_AttributeSet* GetAttributeSet() const;
	UFUNCTION(BlueprintCallable, Category="Crash|Abilities")
	virtual UActionComponent* GetUCombatActionComponent() const;

	virtual void OnHealthChange(const FOnAttributeChangeData& AttributeChangeData);
	

	
protected:

	/** 构造函数创建的独立子对象；状态变化不会写回配置资产。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "Crash|Data")
	TObjectPtr<UCC_CharacterRuntimeData> RuntimeData;
	/** 被感知组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "Crash|Data")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GiveStartupAbilities();

	virtual void InitializeAttributes() const;
	
};
