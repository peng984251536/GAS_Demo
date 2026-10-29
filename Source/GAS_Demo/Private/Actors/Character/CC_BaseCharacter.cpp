// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/Character/CC_BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Components/ActionComponent.h"
#include "Data/CC_CharacterConfig.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GameFramework/PlayerState.h"
#include "GAS_Demo.h"
#include "Net/UnrealNetwork.h"
#include "Subsystems/CC_GameEventSubsystem.h"

#include "Perception/AISense_Sight.h"

namespace CrashTags
{
	const FName Player = FName("Player");
	const FName Follower = FName("Follower");
}

// Sets default values
FCC_AnyCharacterReady ACC_BaseCharacter::OnAnyCharacterReady;

void ACC_BaseCharacter::BroadcastASCInitialized()
{
	bASCInitialized = true;
	OnASCInitialized.Broadcast(GetAbilitySystemComponent(), GetAttributeSet());
	OnAnyCharacterReady.Broadcast(this);
}

ACC_BaseCharacter::ACC_BaseCharacter()
{
	BuffComponent = CreateDefaultSubobject<UCC_BuffComponent>(TEXT("BuffComponent"));
	RuntimeData = CreateDefaultSubobject<UCC_CharacterRuntimeData>(TEXT("CharacterRuntimeData"));
	bReplicateUsingRegisteredSubObjectList = true;
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = true;

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// 防止混战时被推挤或弹飞：
	// 1. 禁止其他角色踩到自己身上（胶囊体和 Mesh 都要设置）
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	GetMesh()->CanCharacterStepUpOn = ECB_No;
	// 2. Mesh 只用于查询（近战 Overlap 仍能命中），不参与 Pawn 之间的物理阻挡
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	// 3. 关闭角色移动对物理体的推力交互
	GetCharacterMovement()->bEnablePhysicsInteraction = false;

	// 被感知组件
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));
	StimuliSource->bAutoRegister = true;
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
}

// Called when the game starts or when spawned
void ACC_BaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ACC_BaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (HasAuthority())
	{
		AddReplicatedSubObject(RuntimeData);
	}
}

void ACC_BaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindOutOfHealth();
	if (HasAuthority())
	{
		RemoveReplicatedSubObject(RuntimeData);
	}
	Super::EndPlay(EndPlayReason);
}

void ACC_BaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACC_BaseCharacter, TeamOverride);
}

void ACC_BaseCharacter::SetTeamOverride(FGameplayTag InTeam)
{
	if (!HasAuthority())
	{
		return;
	}
	TeamOverride = InTeam;
}

const UCC_CharacterConfig* ACC_BaseCharacter::GetCharacterConfig() const
{
	return nullptr;
}

UCC_CharacterRuntimeData* ACC_BaseCharacter::GetRuntimeData() const
{
	return RuntimeData;
}

void ACC_BaseCharacter::SetLastMoveInputDirection(const FVector& InDirection)
{
	// UE_LOG(LogGAS_Demo, Warning,
	// 	TEXT("[BowAI][Facing] Store Character=%s dir=%s"),
	// 	*GetNameSafe(this),
	// 	*InDirection.ToCompactString());
	RuntimeData->SetLastMoveInputDirection(InDirection);
}

FVector ACC_BaseCharacter::GetLastMoveInputDirection() const
{
	return RuntimeData->GetLastMoveInputDirection();
}

bool ACC_BaseCharacter::IsAlive() const
{
	return RuntimeData->IsAlive();
}

bool ACC_BaseCharacter::IsHit() const
{
	return RuntimeData->IsHit();
}

void ACC_BaseCharacter::SetClosestActor(const FClosestActorWithTagResult& InTarget)
{
	if (HasAuthority())
	{
		// UE_LOG(
		// 	LogGAS_Demo,
		// 	Warning,
		// 	TEXT("[Action][AbilityByTag] SetClosestActor tag"));
		RuntimeData->SetClosestActor(InTarget);
	}
}

FClosestActorWithTagResult ACC_BaseCharacter::GetClosestActor() const
{
	return RuntimeData->GetClosestActor();
}

/** 从共享配置读取技能，在现有 ASC 初始化完成后由服务器授予。 */
void ACC_BaseCharacter::GiveStartupAbilities()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC))
	{
		return;
	}

	const UCC_CharacterConfig* Data = GetCharacterConfig();
	const auto GrantAbility = [this, ASC](TSubclassOf<UGameplayAbility> Ability, int32 Level)
	{
		if (!IsValid(Ability) || Ability->HasAnyClassFlags(CLASS_Abstract) || Level < 1)
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("%s: skipping invalid startup ability '%s' (level %d)."),
				*GetName(), *GetNameSafe(Ability.Get()), Level);
			return;
		}

		// 继续走 ASC::GiveAbility，保留项目现有 ActivateOnGiven 自动激活机制。
		// 无缝切图/重生可复用 PlayerState ASC；同类初始能力只授予一次，避免叠加 Spec。
		if (ASC->FindAbilitySpecFromClass(Ability))
		{
			return;
		}
		ASC->GiveAbility(FGameplayAbilitySpec(Ability, Level));
	};
	if (IsValid(Data))
	{
		for (const TSubclassOf<UCC_GameplayAbilityBase> Ability : Data->StartupAbilities)
		{
			GrantAbility(Ability, 1);
		}
		return;
	}

	UE_LOG(LogGAS_Demo, Warning, TEXT("%s: CharacterConfig is not configured; no startup abilities granted."), *GetName());
}

void ACC_BaseCharacter::InitializeAttributes() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC))
	{
		return;
	}
	const UCC_CharacterConfig* Config = GetCharacterConfig();
	const TSubclassOf<UGameplayEffect> EffectClass = IsValid(Config) && Config->InitializeAttributesEffect
		? Config->InitializeAttributesEffect : nullptr;
	if (!IsValid(EffectClass))
	{
		UE_LOG(LogGAS_Demo, Error, TEXT("%s: InitializeAttributesEffect is not configured."), *GetName());
		return;
	}

	// GameplayEffect 是effect的模板
	// 执行的背景信息（谁释放、作用于谁、）
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	// 执行effect的实例
	FGameplayEffectSpecHandle SpecHandle = ASC->
	MakeOutgoingSpec(
		EffectClass,
		1.0f,
		ContextHandle);

	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	
}

UAbilitySystemComponent* ACC_BaseCharacter::GetAbilitySystemComponent() const
{
	return nullptr;
}

UCC_AttributeSet* ACC_BaseCharacter::GetAttributeSet() const
{
	return nullptr;
}
UActionComponent* ACC_BaseCharacter::GetUCombatActionComponent() const
{
	return nullptr;
}

void ACC_BaseCharacter::OnHealthChange(const FOnAttributeChangeData& AttributeChangeData)
{
	// 死亡判定改由 UCC_AttributeSet::OnOutOfHealth 驱动（带击杀者），见 HandleOutOfHealth。
	// 这里不再调用 HandleDeath：属性回调早于来源解析，在这里死亡会丢失击杀者。
}

void ACC_BaseCharacter::BindOutOfHealth(UCC_AttributeSet* Attributes)
{
	if (!HasAuthority() || !IsValid(Attributes))
	{
		return;
	}
	UnbindOutOfHealth();
	OutOfHealthSource = Attributes;
	OutOfHealthHandle = Attributes->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
}

void ACC_BaseCharacter::UnbindOutOfHealth()
{
	if (UCC_AttributeSet* Attributes = OutOfHealthSource.Get(); Attributes && OutOfHealthHandle.IsValid())
	{
		Attributes->OnOutOfHealth.Remove(OutOfHealthHandle);
	}
	OutOfHealthHandle.Reset();
	OutOfHealthSource.Reset();
}

/**
 * 生命耗尽：执行死亡清理，并广播一次带击杀者的死亡通知
 * 
 * @param DamageInstigator 
 * @param DamageCauser 
 * @param EffectSpec 
 * @param Magnitude 
 */
void ACC_BaseCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser,
                                          const FGameplayEffectSpec& EffectSpec, float Magnitude)
{
	if (!HasAuthority() || bDeathReported)
	{
		return;
	}
	// 玩家属性集在 PlayerState 上：Pawn 已换掉时旧 Pawn 不应响应。
	if (GetAttributeSet() != OutOfHealthSource.Get())
	{
		return;
	}
	bDeathReported = true;
	HandleDeath();

	FCC_DeathInfo Info;
	Info.Victim = this;
	Info.Killer = DamageInstigator;
	Info.DamageCauser = DamageCauser;
	if (const APawn* KillerPawn = Cast<APawn>(DamageInstigator))
	{
		Info.KillerPlayerState = KillerPawn->GetPlayerState();
	}
	UE_LOG(LogGAS_Demo, Log, TEXT("[Death] Victim=%s Killer=%s Causer=%s"),
		*GetNameSafe(this), *GetNameSafe(DamageInstigator), *GetNameSafe(DamageCauser));
	if (UCC_GameEventSubsystem* Events = UCC_GameEventSubsystem::Get(this))
	{
		// 告诉其他人 这个死亡了
		Events->BroadcastCharacterDied(Info);
	}
}

void ACC_BaseCharacter::HandleDeathFinished()
{
	if (!HasAuthority())
	{
		return;
	}
	if (UCC_GameEventSubsystem* Events = UCC_GameEventSubsystem::Get(this))
	{
		Events->BroadcastCharacterDeathFinished(this);
	}
}

void ACC_BaseCharacter::HandleDeath()
{
	// 幂等：死亡能力与生命耗尽事件都会调用；尚未出生（bAlive 初值 false）的角色也不重复清理。
	if (!HasAuthority() || !RuntimeData->IsAlive())
	{
		return;
	}
	RuntimeData->SetAlive(false);
	BuffComponent->ClearBuffs(ECC_BuffEndReason::OwnerDeath);
	RuntimeData->SetHit(false);
	RuntimeData->ClearClosestActor();

	// if(GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
	// 		FString::Printf(TEXT("%s has died!"),*GetName()));
	// }
}

void ACC_BaseCharacter::HandleHit()
{
	if (!HasAuthority())
	{
		return;
	}
	RuntimeData->SetHit(true);
	//AttackActor = Character;

	// if(GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
	// 		FString::Printf(TEXT("ACC_BaseCharacter::HandleHit::%s"),*GetName()));
	// }
}
void ACC_BaseCharacter::HandleRefreshHit()
{
	if (!HasAuthority())
	{
		return;
	}
	RuntimeData->SetHit(false);
	//AttackActor = nullptr;
	// if(GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
	// 		FString::Printf(TEXT("ACC_BaseCharacter::HandleRefresh::%s"),*GetName()));
	// }
}

/**
 * 重生（重置属性、能力）
 */
void ACC_BaseCharacter::HandleRespawn()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC) || !HasAuthority())
	{
		return;
	}

	BuffComponent->ClearBuffs();
	RuntimeData->ResetForRespawn();
	ResetDeathReport();
	ForceNetUpdate();

	//清空能力
	//GetAbilitySystemComponent()->CancelAllAbilities();
	//清空效果
	// 空 Query：匹配所有当前仍处于 Active 状态的 GE
	// FGameplayEffectQuery Query;
	// const TArray<FActiveGameplayEffectHandle> Handles =
	// 	ASC->GetActiveEffects(Query);
	// for (const FActiveGameplayEffectHandle& Handle : Handles)
	// {
	// 	ASC->RemoveActiveGameplayEffect(Handle);
	// }

	// 加载能力 启动能力
	//GiveStartupAbilities();
	// 初始化属性能力效果
	InitializeAttributes();
	
}






TArray<FCC_ReplicatedBuffItem> ACC_BaseCharacter::GetActiveBuffs() const
{
    return BuffComponent->GetActiveBuffs();
}
UCombatActionSet* ACC_BaseCharacter::GetEffectiveAttackActionSet() const
{
    const auto* Config = GetCharacterConfig();
    return IsValid(Config) ? Config->AttackActionSet.Get() : nullptr;
}
