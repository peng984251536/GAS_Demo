// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "CC_BaseCharacter.h"


#include "CC_PlayerCharacter.generated.h"


UCLASS()
class GAS_DEMO_API ACC_PlayerCharacter : public ACC_BaseCharacter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	float MyTargetArmLength = 500.f;

	// Sets default values for this character's properties
	ACC_PlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual  UActionComponent* GetUCombatActionComponent() const override;
	virtual void PossessedBy(AController* PlayerController) override;
	virtual void OnRep_PlayerState() override;
	bool TryActivateAbilities(TSubclassOf<UGameplayAbility> AbilityClass) const;
	virtual UCC_AttributeSet* GetAttributeSet() const override;
	
	//蓝图事件
	// UFUNCTION(BlueprintImplementableEvent, Category="Crash|Input")
	// void OnAttackInput();

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;
	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr <USpringArmComponent> CameraBoom;

protected:
	/** 默认从角色蓝图读取；子类可覆盖配置来源。返回只读配置，不存放角色实例状态。 */
	virtual const UCC_CharacterConfig* GetCharacterConfig() const override;
	
};
