#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Input/GASInputTypes.h"
#include "CC_PlayerController.generated.h"

class UInputAction;
class UInputActionValue;
class UEnhancedInputComponent;
class UGASInputConfig;
class UCC_AbilitySystemComponent;
class ACC_PlayerCharacter;
class UCC_BuffSelectionComponent;

UCLASS()
class ACC_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACC_PlayerController();

	/** Buff 三选一流程；跟随控制器而不是 Pawn，重生不会打断进行中的一轮。 */
	UFUNCTION(BlueprintPure, Category="Buff|Selection")
	UCC_BuffSelectionComponent* GetBuffSelection() const { return BuffSelection; }

protected:
	virtual void BeginPlay() override;

	/** 在蓝图 PlayerController 的组件面板里设置 Selection Screen Class。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Buff|Selection")
	TObjectPtr<UCC_BuffSelectionComponent> BuffSelection;
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
