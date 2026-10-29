#include "Actors/Character/CC_PlayerCharacter.h"
#include "Debug/CC_DebugCVars.h"
#include "AI/Crowd/CC_PlayerCrowdAgentComponent.h"
#include "GameplayTags/CC_Tags.h"
#include "GAS_Demo.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/CC_CharacterRuntimeData.h"
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

	// 玩家作为 Crowd 障碍：敌人避让时会把玩家考虑进去。
	CrowdAgent = CreateDefaultSubobject<UCC_PlayerCrowdAgentComponent>(TEXT("CrowdAgent"));

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
	// 属性集在 PlayerState 上，每次被控制都重新绑定生命耗尽事件到当前 Pawn。
	BindOutOfHealth(GetAttributeSet());

	// 广播委托 初始化
	BroadcastASCInitialized();
	
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
	if (GEngine && CCDebug::ShowMessages())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("ACC_PlayerCharacter::OnRep_PlayerState")));
	}

	
	// 能力系统在哪里管理  能力系统在哪里执行
	GetAbilitySystemComponent()->InitAbilityActorInfo(GetPlayerState(),this);

	// 广播委托 初始化
	BroadcastASCInitialized();


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

FGameplayTag ACC_PlayerCharacter::GetDefaultTeam() const
{
	return CCTags::Team::Player;
}

bool ACC_PlayerCharacter::Revive(const FVector& Location, TSubclassOf<UGameplayEffect> ReviveEffect)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsAlive())
	{
		return false;
	}

	// 先标记存活，再取消死亡能力：CC_Death 结束时看到角色已活着就不会再上报 DeathFinished。
	RuntimeData->ResetForRespawn();
	ResetDeathReport();
	const FGameplayTagContainer DeathAbilityTags(CCTags::CCAbilityTrigger::Death);
	ASC->CancelAbilities(&DeathAbilityTags);

	TeleportTo(Location, GetActorRotation(), false, true);
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement(); Movement && Movement->MovementMode == MOVE_None)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (ReviveEffect)
	{
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ReviveEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	// 兜底：没有配置复活 GE 或它没有回血时回满，避免"复活即死亡"。
	if (const UCC_AttributeSet* Attributes = GetAttributeSet(); Attributes && Attributes->GetHealth() < UCC_AttributeSet::OutOfHealthThreshold)
	{
		ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	}

	BuffComponent->ResumeAfterRevive();
	ForceNetUpdate();
	UE_LOG(LogGAS_Demo, Log, TEXT("[Revive] %s at %s"), *GetName(), *Location.ToCompactString());
	return true;
}
