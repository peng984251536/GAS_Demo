// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CC_PlayerState.h"
#include "GAS_Demo.h"
#include "AbilitySystemComponent.h"



ACC_PlayerState::ACC_PlayerState()
{
	//状态的同步频率
	SetNetUpdateFrequency(100.f);

	AbilitySystem = CreateDefaultSubobject<UCC_AbilitySystemComponent>("CC_UAbilitySystemComponent");
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	//连击组件
	CombatActionComponent = CreateDefaultSubobject<UActionComponent>("CombatActionComponent");
	
	// 属性集
	AttributeSet = CreateDefaultSubobject<UCC_AttributeSet>("CC_AttributeSet");

	UE_LOG(LogGAS_Demo, Warning, TEXT("ACC_PlayerState::ACC_PlayerState"));
}

/**
 * 拿到能力系统
 * @return 
 */
UAbilitySystemComponent* ACC_PlayerState::GetAbilitySystemComponent() const
{
	if(!IsValid(AbilitySystem))
		return nullptr;
		
// 		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// FString::Printf(TEXT("ACC_PlayerState::AbilitySystem is null")));
	
	return AbilitySystem;
}
UActionComponent* ACC_PlayerState::GetUCombatActionComponent() const
{
	if(!IsValid(CombatActionComponent))
		return nullptr;
		
	// 		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
	// FString::Printf(TEXT("ACC_PlayerState::AbilitySystem is null")));
	
	return CombatActionComponent;
}

UCC_AttributeSet* ACC_PlayerState::GetAttributeSet() const
{
	return AttributeSet;
}
