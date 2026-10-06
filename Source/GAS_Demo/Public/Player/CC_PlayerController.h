#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Data/GASInputTypes.h"
#include "CC_PlayerController.generated.h"

class UInputAction;
class UInputActionValue;
class UEnhancedInputComponent;
class UGASInputConfig;
class UCC_AbilitySystemComponent;
class ACC_PlayerCharacter;

UCLASS()
class ACC_PlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
 
	// 管理输入的map
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TMap<EGASInputMappingType, FGASInputMappingContextEntry> MappingContexts;

	// 输入配置
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UGASInputConfig> InputConfig = nullptr;

	// action类型
	UPROPERTY(EditDefaultsOnly, Category="Input|Tags")
	FGameplayTag MoveInputTag;
	UPROPERTY(EditDefaultsOnly, Category="Input|Tags")
	FGameplayTag DodgeInputTag;
	UPROPERTY(EditDefaultsOnly, Category="Input|Tags")
	FGameplayTag AttackInputTag;

private:
	void AddInputMapping(EGASInputMappingType MappingType);
	void RemoveInputMapping(EGASInputMappingType MappingType);
	
	void BindNativeInputActions(UEnhancedInputComponent* EnhancedInputComponent);
	void BindNativeAction(
		UEnhancedInputComponent* EnhancedInputComponent,
		const FGameplayTag& InputTag,
		ETriggerEvent TriggerEvent,
		void (ACC_PlayerController::*Func)(const FInputActionValue&)
	);
	
	//void ActivateAbility(const FGameplayTag& AbilityTag) const;
	
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Dodge(const FInputActionValue& InputActionValue);
	void Input_Attack(const FInputActionValue& InputActionValue);
	
};
