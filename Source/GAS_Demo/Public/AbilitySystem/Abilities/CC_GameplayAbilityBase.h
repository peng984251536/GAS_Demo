// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Debug/CC_DebugCVars.h"
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
	/**
	 * 本技能是否参与屏幕调试文字。总开关是控制台变量 cc.Debug.Messages（默认关），
	 * 两者都为真才显示；想单独屏蔽某个吵的技能时在这里取消勾选。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool ShowDebug = true;

	/** C++ 中判断是否输出屏幕调试文字，统一走这里。 */
	bool ShouldShowDebug() const { return ShowDebug && CCDebug::ShowMessages(); }

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
