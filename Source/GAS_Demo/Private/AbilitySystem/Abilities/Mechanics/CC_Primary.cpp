// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Mechanics/CC_Primary.h"
#include "GAS_Demo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Character/CC_BaseCharacter.h"
#include "Character/CC_PlayerCharacter.h"
#include "Engine/OverlapResult.h"
#include "Components/ActionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "GameplayTags/CC_Tags.h"





void UCC_Primary::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}
	if (!IsValid(CombatComponent) || !IsValid(OwnedAction) || !IsValid(OwnedAction->Montage))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Attack requires an action component and action montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	
	// 播放蒙太奇
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("PlayActionMontage"),
		OwnedAction->Montage,
		1.0f,
		NAME_None,
		true);

	if (!IsValid(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 只有确认能创建播放任务后，才提交连招状态。
	OwnedGeneration = CombatComponent->GetAttackIndex(OwnedAction);

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();
	
	// 方式二：屏幕打印，调试时更直观
	if (GEngine && ShowDebug)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("Ability Activated: %s"), *GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("Ability Activated: %s"), *GetName()));
	}
	
}

#pragma region hit目标
TArray<FHitResultAndActor> UCC_Primary::HitBoxOverlapText()
{
	TArray<FHitResultAndActor> Hits;
	AActor* Attacker = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(Attacker) || !World)
	{
		return Hits;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Attacker);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Overlap);

	const FVector AttackOrigin = Attacker->GetActorLocation();
	const FVector HitBoxLocation = AttackOrigin
		+ Attacker->GetActorForwardVector() * HitBoxForwardOffset;
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, HitBoxLocation, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(HitBoxRadius), QueryParams, ResponseParams);

	TSet<AActor*> AddedActors;
	FCollisionQueryParams TraceParams;
	TraceParams.bTraceComplex = false;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsValid(Target) || !IsValid(Component) || AddedActors.Contains(Target))
		{
			continue;
		}

		const FVector Center = Component->Bounds.Origin;
		FVector TowardAttacker = (AttackOrigin - Center).GetSafeNormal();
		if (TowardAttacker.IsNearlyZero())
		{
			TowardAttacker = -Attacker->GetActorForwardVector();
		}
		// 补射线取得朝攻击者一侧的碰撞表面；保留原有检测方式。
		const float Distance = Component->Bounds.SphereRadius + 10.f;
		FHitResult Hit;
		if (!Component->LineTraceComponent(Hit, Center + TowardAttacker * Distance,
			Center - TowardAttacker * Distance, TraceParams) || Hit.bStartPenetrating)
		{
			continue;
		}

		FHitResultAndActor& Entry = Hits.AddDefaulted_GetRef();
		Entry.Result = Hit;
		Entry.hitActor = Target;
		// 成功后再去重，允许失败时尝试该角色的其他组件。
		AddedActors.Add(Target);
	}

	if (ShowDebug)
	{
		DrawHitBoxOverlapDebugs(Hits, HitBoxLocation);
	}
	return Hits;
}

void UCC_Primary::DrawHitBoxOverlapDebugs(
	const TArray<FHitResultAndActor>& Hits, const FVector& HitBoxLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DrawDebugSphere(World, HitBoxLocation, HitBoxRadius, 16, FColor::Red, false, 5.f);
	for (const FHitResultAndActor& Hit : Hits)
	{
		if (!IsValid(Hit.hitActor))
		{
			continue;
		}
		const FVector Position = Hit.Result.ImpactPoint;
		DrawDebugSphere(World, Position, 8.f, 10, FColor::Green, false, 3.f);
		DrawDebugDirectionalArrow(World, Position, Position + Hit.Result.ImpactNormal * 50.f,
			10.f, FColor::Yellow, false, 3.f);
	}
}

void UCC_Primary::SendEventToActor(const TArray<FHitResultAndActor>& Hits)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(SourceASC))
	{
		return;
	}

	TSet<AActor*> SentActors;
	for (const FHitResultAndActor& Hit : Hits)
	{
		ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Hit.hitActor);
		if (!IsValid(Character) || SentActors.Contains(Character))
		{
			continue;
		}
		UActionComponent* TargetActions = Character->GetUCombatActionComponent();
		UAbilitySystemComponent* TargetASC = Character->GetAbilitySystemComponent();
		if (!IsValid(TargetActions) || !IsValid(TargetASC))
		{
			continue;
		}

		const FGameplayAbilityTargetDataHandle TargetData =
			UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit.Result);
		SentActors.Add(Character);
		TargetActions->TryGameplayAbilityByTag(
			CCTags::CCAbilityTrigger::BeHit, SourceASC, TargetASC, TargetData);
	}
}
#pragma endregion
#pragma region 攻击时旋转

void UCC_Primary::SetAttackFacingFromAvatarInput(float InterpSpeed)
{
	const ACC_BaseCharacter* Character =
	Cast<ACC_BaseCharacter>(
		GetAvatarActorFromActorInfo());

	if (!Character)
	{
		return;
	}

	SetAttackFacing(
		Character->GetLastMoveInputDirection(),
		InterpSpeed
	);
}

void UCC_Primary::SetAttackFacing(FVector InputDirection,float InterpSpeed)
{
	
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	// 只处理地面朝向，忽略 Z。
	FVector FlatDirection = InputDirection;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		return;
	}

	FlatDirection.Normalize();

	const FRotator TargetRotation =
		FRotator(0.0f, FlatDirection.Rotation().Yaw, 0.0f);

	const float DeltaSeconds = GetWorld()
		? GetWorld()->GetDeltaSeconds()
		: 0.0f;

	const FRotator NewRotation =
		InterpSpeed > 0.0f && DeltaSeconds > 0.0f
		? FMath::RInterpTo(
			AvatarActor->GetActorRotation(),
			TargetRotation,
			DeltaSeconds,
			InterpSpeed)
		: TargetRotation;

	AvatarActor->SetActorRotation(NewRotation);
}
#pragma endregion 


