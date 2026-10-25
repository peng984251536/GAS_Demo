// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Character/CC_EnemyCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Player/CC_PlayerState.h"

#pragma region  生命周期
// Sets default values
ACC_EnemyCharacter::ACC_EnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	Tags.Add(CrashTags::Follower);
	
	AbilitySystemComponent = CreateDefaultSubobject<UCC_AbilitySystemComponent>("CC_UAbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	//AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	//连击组件
	CombatActionComponent = CreateDefaultSubobject<UActionComponent>("CombatActionComponent");
	// 属性集
	AttributeSet = CreateDefaultSubobject<UCC_AttributeSet>("CC_AttributeSet");

	AAIController* AI = Cast<AAIController>(GetController());
	UAIPerceptionComponent* Perception = AI ? AI->GetAIPerceptionComponent() : nullptr;
	// 取得该组件中配置的 Sight（视觉）感知设置。
	UAISenseConfig_Sight* Sight =
		Perception ? Perception->GetSenseConfig<UAISenseConfig_Sight>() : nullptr;
	if (Sight)
	{
		Sight->SightRadius = ACC_EnemyCharacter::GetCharacterConfig()->MaxRange;
		Sight->LoseSightRadius = ACC_EnemyCharacter::GetCharacterConfig()->MaxRange+100;
		//Sight->PeripheralVisionAngleDegrees = 60.0f; // 左右各 60°，总视角 120°

		Perception->ConfigureSense(*Sight);
		Perception->RequestStimuliListenerUpdate();
	}
}

// Called when the game starts or when spawned
void ACC_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetAbilitySystemComponent()->InitAbilityActorInfo(this,this);

	// 广播委托 初始化（头顶血条等 UI 订阅 ACC_BaseCharacter::OnAnyCharacterReady 自行处理）
	BroadcastASCInitialized();

	//-----后续是服务器相关的初始化----//
	if(!HasAuthority()) return;
	
	// 避障由 ACC_AIController 的 UCrowdFollowingComponent（Detour Crowd）负责。
	// 不要再开启 CharacterMovement 的 RVO：两套避障同时生效会互相打架。

	// 加载能力 启动能力
	GiveStartupAbilities();
	// 重生时：重新启动能力、重新初始化属性能力
	HandleRespawn();
	// 死亡判定入口：生命耗尽事件带击杀者（见 ACC_BaseCharacter::HandleOutOfHealth）。
	BindOutOfHealth(AttributeSet);

	FOnGameplayAttributeValueChange& ChangeDelegate =
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(
		GetAttributeSet()->GetHealthAttribute());
	// lambda表达式
	ChangeDelegate.AddUObject(this,
		&ThisClass::OnHealthChange);
}

// 启动能力
void ACC_EnemyCharacter::GiveStartupAbilities()
{
	Super::GiveStartupAbilities();
}

// Called every frame
void ACC_EnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDrawPerceptionSightCone)
	{
		// if (const UAIPerceptionComponent* PerceptionComponent = FindComponentByClass<UAIPerceptionComponent>())
		// {
		// 	if (const UAISenseConfig_Sight* SightConfig = PerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>())
		// 	{
		// 		FVector ViewLocation;
		// 		FRotator ViewRotation;
		// 		GetActorEyesViewPoint(ViewLocation, ViewRotation);
		//
		// 		// 感知范围在场景中以贴地扇形表示，比三维圆锥更便于观察。
		// 		const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, 8.0f);
		// 		FVector Forward = ViewRotation.Vector();
		// 		Forward.Z = 0.0f;
		// 		Forward.Normalize();
		//
		// 		const float HalfAngleDegrees = SightConfig->PeripheralVisionAngleDegrees;
		// 		constexpr int32 ArcSegments = 32;
		// 		FVector PreviousPoint = Origin + Forward.RotateAngleAxis(-HalfAngleDegrees, FVector::UpVector) * SightConfig->SightRadius;
		// 		DrawDebugLine(GetWorld(), Origin, PreviousPoint, SightConfig->GetDebugColor(), false, 0.0f, 0, 1.5f);
		//
		// 		for (int32 Segment = 1; Segment <= ArcSegments; ++Segment)
		// 		{
		// 			const float Alpha = static_cast<float>(Segment) / ArcSegments;
		// 			const float Angle = FMath::Lerp(-HalfAngleDegrees, HalfAngleDegrees, Alpha);
		// 			const FVector CurrentPoint = Origin + Forward.RotateAngleAxis(Angle, FVector::UpVector) * SightConfig->SightRadius;
		// 			DrawDebugLine(GetWorld(), PreviousPoint, CurrentPoint, SightConfig->GetDebugColor(), false, 0.0f, 0, 1.5f);
		// 			PreviousPoint = CurrentPoint;
		// 		}
		//
		// 		DrawDebugLine(GetWorld(), Origin, PreviousPoint, SightConfig->GetDebugColor(), false, 0.0f, 0, 1.5f);
		// 	}
		// }

		// // 橙色圆圈表示弓兵的最小射程；目标进入圈内时应由 KeepDistance 尝试后撤。
		// // 与视野扇形一样每帧重画，怪物移动时圆圈会跟随，关闭调试开关后也不会残留。
		// if (IsValid(CharacterConfig) && CharacterConfig->MaxRange > 0.0f)
		// {
		// 	
		// 	constexpr int32 CircleSegments = 64;
		// 	const FVector CircleCenter = GetActorLocation() + FVector(0.0f, 0.0f, 8.0f);
		// 	FVector PreviousPoint = CircleCenter + FVector::ForwardVector * CharacterConfig->MaxRange;
		// 	for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
		// 	{
		// 		const float Angle = 360.0f * static_cast<float>(Segment) / CircleSegments;
		// 		const FVector CurrentPoint = CircleCenter
		// 			+ FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * CharacterConfig->MaxRange;
		// 		DrawDebugLine(GetWorld(), PreviousPoint, CurrentPoint, FColor::Red, false, 0.0f, 0, 2.0f);
		// 		PreviousPoint = CurrentPoint;
		// 	}
		// }
		//
		// // 橙色圆圈表示弓兵的最小射程；目标进入圈内时应由 KeepDistance 尝试后撤。
		// // 与视野扇形一样每帧重画，怪物移动时圆圈会跟随，关闭调试开关后也不会残留。
		// if (IsValid(CharacterConfig) && CharacterConfig->MinRange > 0.0f)
		// {
		// 	
		// 	constexpr int32 CircleSegments = 64;
		// 	const FVector CircleCenter = GetActorLocation() + FVector(0.0f, 0.0f, 8.0f);
		// 	FVector PreviousPoint = CircleCenter + FVector::ForwardVector * CharacterConfig->MinRange;
		// 	for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
		// 	{
		// 		const float Angle = 360.0f * static_cast<float>(Segment) / CircleSegments;
		// 		const FVector CurrentPoint = CircleCenter
		// 			+ FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * CharacterConfig->MinRange;
		// 		DrawDebugLine(GetWorld(), PreviousPoint, CurrentPoint, FColor::Orange, false, 0.0f, 0, 2.0f);
		// 		PreviousPoint = CurrentPoint;
		// 	}
		// }
	}

	// if(HasAuthority() && IsValid( FClosestActor.Actor.Get()))
	// {
	// 	constexpr float LoseTargetDistance = 350.0f;
	// 	const float DistanceSquared = FVector::DistSquared(
	// 		FClosestActor.Actor.Get()->GetActorLocation(),
	// 		GetActorLocation());
	//
	// 	if (DistanceSquared > FMath::Square(LoseTargetDistance))
	// 	{
	// 		SetClosestActor(FClosestActorWithTagResult{});
	// 	}
	//
	// 	const float Distance = FMath::Sqrt(DistanceSquared);
	// 	DrawDebugString(
	// 		GetWorld(),
	// 		GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
	// 		FString::Printf(TEXT("Distance: %.0f"), Distance),
	// 		nullptr,
	// 		FColor::Yellow,
	// 		0.0f,
	// 		true
	// 	);
	// }
	
}


#pragma endregion 

#pragma region  Data
/**
 * 从角色状态信息里拿到ASC
 * @return 
 */
UAbilitySystemComponent* ACC_EnemyCharacter::GetAbilitySystemComponent() const
{
	if(!IsValid(AbilitySystemComponent))
		return nullptr;

	return AbilitySystemComponent;
}
UCC_AttributeSet* ACC_EnemyCharacter::GetAttributeSet() const
{
	return AttributeSet;
}
UActionComponent* ACC_EnemyCharacter::GetUCombatActionComponent() const
{
	if(!IsValid(CombatActionComponent))
		return nullptr;

	return CombatActionComponent;
}

void ACC_EnemyCharacter::HandleDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	Super::HandleDeath();

	if (AAIController* EnemyController = Cast<AAIController>(GetController()))
	{
		EnemyController->StopMovement();
	}
}

void ACC_EnemyCharacter::HandleRespawn()
{
	Super::HandleRespawn();

}

void ACC_EnemyCharacter::SetSpawnParams(const FCC_EnemySpawnParams& InParams)
{
	// 只在出生前（FinishSpawning 之前）写入才会影响属性初始化。
	ensureMsgf(!HasActorBegunPlay(), TEXT("%s: SpawnParams must be set before FinishSpawning."), *GetName());
	SpawnParams = InParams;
	SpawnParams.bIsSet = true;
}

float ACC_EnemyCharacter::GetExperienceReward() const
{
	const float Base = IsValid(CharacterConfig) ? CharacterConfig->ExperienceReward : 0.f;
	return FMath::Max(0.f, Base * (SpawnParams.bIsSet ? SpawnParams.ExperienceMultiplier : 1.f));
}

void ACC_EnemyCharacter::InitializeAttributes() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!SpawnParams.bIsSet || !HasAuthority() || !IsValid(ASC))
	{
		Super::InitializeAttributes();
		return;
	}

	if (SpawnParams.InitEffect)
	{
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			SpawnParams.InitEffect, FMath::Max(1.f, SpawnParams.EffectLevel), Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	else
	{
		Super::InitializeAttributes();
	}

	// 人数缩放：直接改基础值并回满，不需要额外的 GE 资产。先改上限，再写当前值，避免被裁剪。
	if (!FMath::IsNearlyEqual(SpawnParams.HealthScale, 1.f) && SpawnParams.HealthScale > 0.f)
	{
		const float ScaledMax = ASC->GetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute()) * SpawnParams.HealthScale;
		ASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), ScaledMax);
		ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), ScaledMax);
	}
}


const UCC_CharacterConfig* ACC_EnemyCharacter::GetCharacterConfig() const
{
	return  CharacterConfig;
}
#pragma endregion 

FGameplayTag ACC_EnemyCharacter::GetDefaultTeam() const
{
	return CCTags::Team::Enemy;
}
