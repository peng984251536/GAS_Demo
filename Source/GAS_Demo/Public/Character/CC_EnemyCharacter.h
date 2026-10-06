// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CC_BaseCharacter.h"
#include "Attribute/CC_AttributeSet.h"
#include "Components/ActionComponent.h"
#include "Utils/CC_BlueprintLibrary.h"
#include "CC_EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UCC_AbilitySystemComponent;
class UCC_AttributeSet;
class ACC_BaseCharacter;
class AAIController;

UCLASS()
class GAS_DEMO_API ACC_EnemyCharacter : public ACC_BaseCharacter
{
	GENERATED_BODY()

public:
	
	// Sets default values for this character's properties
	ACC_EnemyCharacter();

	/** 是否在游戏视口绘制 AI 视野扇形与 MinRange 圆圈，仅用于调试。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|AI Perception")
	bool bDrawPerceptionSightCone{true};
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual UCC_AttributeSet* GetAttributeSet() const override;
	virtual  UActionComponent* GetUCombatActionComponent() const override;
	virtual const UCC_CharacterConfig* GetCharacterConfig() const override;
	
	virtual void HandleDeath() override;
	virtual void HandleRespawn() override;
	
protected:
	/** 多个角色蓝图可引用同一份配置。当前只支持出生前在类默认值中指定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crash|Data")
	TObjectPtr<UCC_CharacterConfig> CharacterConfig;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void GiveStartupAbilities() override;
	
private:
	/** 后撤决策的节流计时；只在服务器且目标进入 MinRange 后尝试激活能力。 */
	float KeepDistanceCheckRemaining = 0.0f;
	// 可见但是不可编辑
	UPROPERTY(VisibleAnywhere,Category="Crash|Abilities")
	TObjectPtr<UCC_AbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crash|Abilities", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCC_AttributeSet> AttributeSet;
	UPROPERTY(VisibleAnywhere,Category="Crash|Abilities")
	TObjectPtr<UActionComponent> CombatActionComponent;
};
