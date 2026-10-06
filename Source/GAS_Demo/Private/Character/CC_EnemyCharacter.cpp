// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CC_EnemyCharacter.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Character/Combat/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Attribute/CC_AttributeSet.h"
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

	// 广播委托 初始化
	OnASCInitialized.Broadcast(
	GetAbilitySystemComponent(),
	GetAttributeSet());

	//-----后续是服务器相关的初始化----//
	if(!HasAuthority()) return;
	
	// RVO 在本地 AI 移动时处理其他正在移动的 Pawn；寻路位置仍由导航网格决定。
	// 仅为配置了箭的弓兵启用，其他敌人的移动设置不受影响。
	// if (IsValid(CharacterConfig) && CharacterConfig->ArrowProjectileClass)
	// {
	// 	// 用组件接口同时完成避障管理器注册，不能只修改布尔字段。
	// 	GetCharacterMovement()->SetAvoidanceEnabled(true);
	// }

	// 加载能力 启动能力
	GiveStartupAbilities();
	// 重生时：重新启动能力、重新初始化属性能力
	HandleRespawn();
	
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

		// 橙色圆圈表示弓兵的最小射程；目标进入圈内时应由 KeepDistance 尝试后撤。
		// 与视野扇形一样每帧重画，怪物移动时圆圈会跟随，关闭调试开关后也不会残留。
		if (IsValid(CharacterConfig) && CharacterConfig->MaxRange > 0.0f)
		{
			
			constexpr int32 CircleSegments = 64;
			const FVector CircleCenter = GetActorLocation() + FVector(0.0f, 0.0f, 8.0f);
			FVector PreviousPoint = CircleCenter + FVector::ForwardVector * CharacterConfig->MaxRange;
			for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
			{
				const float Angle = 360.0f * static_cast<float>(Segment) / CircleSegments;
				const FVector CurrentPoint = CircleCenter
					+ FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * CharacterConfig->MaxRange;
				DrawDebugLine(GetWorld(), PreviousPoint, CurrentPoint, FColor::Red, false, 0.0f, 0, 2.0f);
				PreviousPoint = CurrentPoint;
			}
		}
		
		// 橙色圆圈表示弓兵的最小射程；目标进入圈内时应由 KeepDistance 尝试后撤。
		// 与视野扇形一样每帧重画，怪物移动时圆圈会跟随，关闭调试开关后也不会残留。
		if (IsValid(CharacterConfig) && CharacterConfig->MinRange > 0.0f)
		{
			
			constexpr int32 CircleSegments = 64;
			const FVector CircleCenter = GetActorLocation() + FVector(0.0f, 0.0f, 8.0f);
			FVector PreviousPoint = CircleCenter + FVector::ForwardVector * CharacterConfig->MinRange;
			for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
			{
				const float Angle = 360.0f * static_cast<float>(Segment) / CircleSegments;
				const FVector CurrentPoint = CircleCenter
					+ FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * CharacterConfig->MinRange;
				DrawDebugLine(GetWorld(), PreviousPoint, CurrentPoint, FColor::Orange, false, 0.0f, 0, 2.0f);
				PreviousPoint = CurrentPoint;
			}
		}
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


const UCC_CharacterConfig* ACC_EnemyCharacter::GetCharacterConfig() const
{
	return  CharacterConfig;
}
#pragma endregion 



