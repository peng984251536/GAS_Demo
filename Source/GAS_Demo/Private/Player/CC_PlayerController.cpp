#include "Player/CC_PlayerController.h"
#include "GAS_Demo.h"
#include "Character/CC_BaseCharacter.h"
#include "Character/CC_PlayerCharacter.h"
#include "GameplayTags/CC_Tags.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Data/GASInputConfig.h"
#include "Utils/CC_BlueprintLibrary.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Components/ActionComponent.h"
#include "GameFramework/Character.h"


void ACC_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	AddInputMapping(EGASInputMappingType::Player);
}




#pragma region 输入相关

// 启动输入组件
void ACC_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 给控制器、角色绑定“增强输入动作”
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("%s: Expected EnhancedInputComponent, got %s"),
			*GetName(), *GetNameSafe(InputComponent));
		return;
	}

	BindNativeInputActions(EnhancedInputComponent);
}


/**
 * 绑定 输入映射
 * @param EnhancedInputComponent 
 */
void ACC_PlayerController::BindNativeInputActions(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!InputConfig)
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("%s: InputConfig is not assigned"), *GetName());
		return;
	}

	// Input setup can run before BeginPlay. Populate the lookup before binding.
	InputConfig->BuildCache();

	BindNativeAction(EnhancedInputComponent, MoveInputTag, ETriggerEvent::Triggered, &ThisClass::Input_Move);
	BindNativeAction(EnhancedInputComponent, DodgeInputTag, ETriggerEvent::Started, &ThisClass::Input_Dodge);
	BindNativeAction(EnhancedInputComponent, AttackInputTag, ETriggerEvent::Started, &ThisClass::Input_Attack);

}

/**
 * 
 * @param EnhancedInputComponent 
 * @param InputTag 
 * @param TriggerEvent 
 * @param Func 
 */
void ACC_PlayerController::BindNativeAction(
	UEnhancedInputComponent* EnhancedInputComponent,
	const FGameplayTag& InputTag,
	ETriggerEvent TriggerEvent,
	void (ACC_PlayerController::*Func)(const FInputActionValue&)
)
{
	if (!EnhancedInputComponent || !InputTag.IsValid())
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("%s: Cannot bind input tag '%s': invalid component or tag"),
			*GetName(), *InputTag.ToString());
		return;
	}

	const UInputAction* InputAction = InputConfig->FindNativeInputAction(InputTag);
	if (!InputAction)
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("%s: No NativeInputAction for tag '%s' in %s"),
			*GetName(), *InputTag.ToString(), *GetNameSafe(InputConfig.Get()));
		return;
	}

	EnhancedInputComponent->BindAction(InputAction, TriggerEvent, this, Func);
	UE_LOG(LogGAS_Demo, Log, TEXT("%s: Bound input tag '%s' to %s"),
		*GetName(), *InputTag.ToString(), *GetNameSafe(InputAction));
}

// void ACC_PlayerController::ActivateAbility(const FGameplayTag& AbilityTag) const
// {
// 	UAbilitySystemComponent* ASC =
// 		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
// 	if(!IsValid(ASC))
// 		return;
//
//
// 	// GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// 	// 	FString::Printf(TEXT("ACC_PlayerController::ActivateAbility:%s"), *AbilityTag.GetTagName().ToString()));
// 	
// 	ASC->TryActivateAbilitiesByTag(AbilityTag.GetSingleTagContainer());
// }

/**
 * 添加输入映射
 * @param MappingType 
 */
void ACC_PlayerController::AddInputMapping(EGASInputMappingType MappingType)
{
	const FGASInputMappingContextEntry* Entry = MappingContexts.Find(MappingType);
	if (!Entry || !Entry->MappingContext)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!Subsystem)
	{
		return;
	}

	// 添加 UInputMappingContext输入映射
	Subsystem->AddMappingContext(Entry->MappingContext, Entry->Priority);
}
void ACC_PlayerController::RemoveInputMapping(EGASInputMappingType MappingType)
{
	const FGASInputMappingContextEntry* Entry = MappingContexts.Find(MappingType);
	if (!Entry || !Entry->MappingContext)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!Subsystem)
	{
		return;
	}

	// 移除 UInputMappingContext输入映射
	Subsystem->RemoveMappingContext(Entry->MappingContext);
}

#pragma endregion


#pragma region 操作行为

void ACC_PlayerController::Input_Move(const FInputActionValue& InputActionValue)
{
	// 获取输入信息
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();
	
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector DesiredMoveDirection(
			MovementVector.Y, // World X: forward/back
			MovementVector.X, // World Y: right/left
			0.0f
		);
	const FVector MoveScale = DesiredMoveDirection.GetSafeNormal();

	if (ACC_PlayerCharacter* playerCharacter =
	Cast<ACC_PlayerCharacter>(ControlledPawn))
	{
		playerCharacter->SetLastMoveInputDirection(MoveScale);
	}
	
	// 只表达“玩家有移动意图”。
	// GA_Attack 只有在 MoveCancelOpen 后才响应它。
	FGameplayEventData Payload;
	Payload.Instigator = ControlledPawn;
	Payload.Target = ControlledPawn;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		ControlledPawn,
		FGameplayTag::RequestGameplayTag(
			TEXT("CCTags.Event.MoveInput")),
		Payload
	);
	
	
	ControlledPawn->AddMovementInput(MoveScale,1.0f,false);
}

void ACC_PlayerController::Input_Dodge(const FInputActionValue& InputActionValue)
{
	APawn* ControlledPawn = GetPawn();

	if(!IsValid(ControlledPawn))
	{
		return;
	}
	ACC_PlayerCharacter* character = Cast<ACC_PlayerCharacter>(ControlledPawn);
	if(!IsValid(character))
	{
		return;
	}

	UE_LOG(LogGAS_Demo, Warning, TEXT("send character configuration."));
	
	character->GetUCombatActionComponent()->TryDodge(
		CCTags::CCAbilityTrigger::DodgeAction);
}

void ACC_PlayerController::Input_Attack(const FInputActionValue& InputActionValue)
{
	APawn* ControlledPawn = GetPawn();
	if(!IsValid(ControlledPawn))
	{
		return;
	}
	ACC_PlayerCharacter* character = Cast<ACC_PlayerCharacter>(ControlledPawn);
	if(!IsValid(character))
	{
		return;
	}

	
	// UAbilitySystemComponent* ASC =
	// 	UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
	// UCC_AbilitySystemComponent* UCC_ASC = Cast<UCC_AbilitySystemComponent>(ASC);
	// if (!UCC_ASC)
	// {
	// 	return;
	// }
	
	// UCC_ASC->HandleComboInput(
	// 	CCTags::CCAbilities::Attack);
	character->GetUCombatActionComponent()->TryAttack
	(CCTags::CCAbilityTrigger::AttackAction);
	
	// FGameplayEventData Payload;
	// Payload.Instigator = ControlledPawn;
	// Payload.Target = ControlledPawn;
	// UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
	// 	ControlledPawn,
	// 	FGameplayTag::RequestGameplayTag(
	// 		TEXT("CCTags.Event.AttackInput")),
	// 	Payload
	// );

	
	//ActivateAbility(CCTags::CCAbilities::Attack);
}


#pragma endregion 


