// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"

#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Components/ActionComponent.h"
#include "Data/CC_CharacterConfig.h"
#include "GameFramework/PlayerState.h"
#include "CC_PlayerState.generated.h"

class UActionComponent;
class UAbilitySystemComponent;
class UCC_AbilitySystemComponent;
class UCC_AttributeSet;

/**
 * 角色的状态
 */
UCLASS()
class GAS_DEMO_API ACC_PlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACC_PlayerState();

	/** 多个角色蓝图可引用同一份配置。当前只支持出生前在类默认值中指定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crash|Data")
	TObjectPtr<UCC_CharacterConfig> CharacterConfig;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UActionComponent* GetUCombatActionComponent() const;
	UFUNCTION(BlueprintPure, Category="Crash|Abilities")
	UCC_AttributeSet* GetAttributeSet() const;
	
private:
	//玩家的能力系统应该跟“玩家”绑定，而不是跟“这具身体”绑定。
	//UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crash|Abilities")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crash|Abilities", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCC_AbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crash|Abilities", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCC_AttributeSet> AttributeSet;
	UPROPERTY(VisibleAnywhere,Category="Crash|Abilities")
	TObjectPtr<UCC_AbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere,Category="Crash|Abilities")
	TObjectPtr<UActionComponent> CombatActionComponent;


};



