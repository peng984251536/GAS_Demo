#include "Character/CC_PlayerCharacter.h"
#include "GAS_Demo.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/CC_PlayerState.h"


// Sets default values
ACC_PlayerCharacter::ACC_PlayerCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	Tags.Add(CrashTags::Player);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	//GetCharacterMovement()->bConstrainToPlane = true;
	//GetCharacterMovement()->bSnapToPlaneAtStart = true;
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = .35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom1"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = MyTargetArmLength;
	CameraBoom->SetRelativeRotation(FRotator(-80.f, 0.f, 0.f));
	// CameraBoom->bUsePawnControlRotation = true;
	// CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	//CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera1"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	
}

/**
 * s 需要同步 Abilities
 * @param PlayerController 
 */
void ACC_PlayerCharacter::PossessedBy(AController* PlayerController)
{
	Super::PossessedBy(PlayerController);

	//判断是否有组件 + 是不是服务器
	if(!IsValid(GetAbilitySystemComponent()) || !HasAuthority())
		return;

	// 能力系统在哪里管理  能力系统在哪里执行
	GetAbilitySystemComponent()->InitAbilityActorInfo(GetPlayerState(),this);
	// 加载能力 启动能力
	GiveStartupAbilities();
	// 重生时：重新启动能力、重新初始化属性能力
	HandleRespawn();

	// 广播委托 初始化
	OnASCInitialized.Broadcast(
	GetAbilitySystemComponent(),
	GetAttributeSet());
	
	FOnGameplayAttributeValueChange& ChangeDelegate =
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(
		GetAttributeSet()->GetHealthAttribute());
	// lambda表达式
	ChangeDelegate.AddUObject(this,
		&ThisClass::OnHealthChange);

	// 初始化属性能力效果 -
	//InitializeAttributes();
}

/**
 * c 同步角色状态
 */
void ACC_PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if(!IsValid(GetAbilitySystemComponent()))
		return;

	UE_LOG(LogGAS_Demo, Log, TEXT("ACC_PlayerCharacter::OnRep_PlayerState"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
	FString::Printf(TEXT("ACC_PlayerCharacter::OnRep_PlayerState")));

	
	// 能力系统在哪里管理  能力系统在哪里执行
	GetAbilitySystemComponent()->InitAbilityActorInfo(GetPlayerState(),this);

	// 广播委托 初始化
	OnASCInitialized.Broadcast(GetAbilitySystemComponent(),GetAttributeSet());


}

#pragma region

bool ACC_PlayerCharacter::TryActivateAbilities(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	check(ASC);

	return ASC->TryActivateAbilityByClass(AbilityClass);
}

UCC_AttributeSet* ACC_PlayerCharacter::GetAttributeSet() const
{
	const ACC_PlayerState* state = Cast<ACC_PlayerState>(GetPlayerState());

	if(!IsValid(state))
		return nullptr;

	return state->GetAttributeSet();
}

#pragma endregion 

#pragma region  Data

const UCC_CharacterConfig* ACC_PlayerCharacter::GetCharacterConfig() const
{
	ACC_PlayerState* state = Cast<ACC_PlayerState>(GetPlayerState());

	if(!IsValid(state))
		return nullptr;

	return state->CharacterConfig;
}

/**
 * 从角色状态信息里拿到ASC
 * @return 
 */
UAbilitySystemComponent* ACC_PlayerCharacter::GetAbilitySystemComponent() const
{
	ACC_PlayerState* state = Cast<ACC_PlayerState>(GetPlayerState());

	if(!IsValid(state))
		return nullptr;

	return state->GetAbilitySystemComponent();
}
UActionComponent* ACC_PlayerCharacter::GetUCombatActionComponent() const
{
	ACC_PlayerState* state = Cast<ACC_PlayerState>(GetPlayerState());

	if(!IsValid(state))
		return nullptr;

	return state->GetUCombatActionComponent();
}

#pragma endregion 

