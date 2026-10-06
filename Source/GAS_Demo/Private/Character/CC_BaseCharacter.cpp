// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/CC_BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Components/ActionComponent.h"
#include "Data/CC_CharacterConfig.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GAS_Demo.h"

#include "Perception/AISense_Sight.h"

namespace CrashTags
{
	const FName Player = FName("Player");
	const FName Follower = FName("Follower");
}

// Sets default values
ACC_BaseCharacter::ACC_BaseCharacter()
{
	RuntimeData = CreateDefaultSubobject<UCC_CharacterRuntimeData>(TEXT("CharacterRuntimeData"));
	bReplicateUsingRegisteredSubObjectList = true;
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = true;

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

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
	if (HasAuthority())
	{
		RemoveReplicatedSubObject(RuntimeData);
	}
	Super::EndPlay(EndPlayReason);
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
	UE_LOG(LogGAS_Demo, Warning,
		TEXT("[BowAI][Facing] Store Character=%s dir=%s"),
		*GetNameSafe(this),
		*InDirection.ToCompactString());
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
	if(AttributeChangeData.NewValue<=0.1f)
	{
		HandleDeath();
	}
}

void ACC_BaseCharacter::HandleDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	RuntimeData->SetAlive(false);
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

	RuntimeData->ResetForRespawn();
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





