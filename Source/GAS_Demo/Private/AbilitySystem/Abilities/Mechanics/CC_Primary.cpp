// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Mechanics/CC_Primary.h"
#include "GAS_Demo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Engine/World.h"
#include "Components/ActionComponent.h"
#include "GameplayTags/CC_Tags.h"
#include "Utils/Team/CC_TeamStatics.h"
#include "Animation/Notifies/AnimNotify_MeleeHitBox.h"

UCC_Primary::UCC_Primary()
{
	// 激活期间给 ASC 挂上「正在攻击」状态，能力以任何方式结束都会自动移除；连段期间计数不归零。
	// 使用者：
	//   - ACC_AIController：0→1 切 Crowd ObstacleOnly（站稳不被推），1→0 恢复避障；攻击中不做被挡检测；
	//   - CC_KeepDistance / CC_Dash：ActivationBlockedTags，攻击期间不后撤 / 不冲刺。
	ActivationOwnedTags.AddTag(CCTags::CCAbilities::Attack);
}





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
TArray<FHitResultAndActor> UCC_Primary::HitBoxOverlapFromEvent(const FGameplayEventData& Payload) const
{
	TArray<FHitResultAndActor> Hits;
	// 只接受命中通知发出的事件，使用事件内的检测快照。
	if (!UAnimNotify_MeleeHitBox::FromEventPayload(Payload)) return Hits;
	TSet<AActor*> AddedActors;
	for (int32 Index = 0; Index < Payload.TargetData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Data = Payload.TargetData.Get(Index);
		const FHitResult* Hit = Data ? Data->GetHitResult() : nullptr;
		AActor* Target = Hit ? Hit->GetActor() : nullptr;
		if (!IsValid(Target) || Target == Payload.Instigator.Get() || AddedActors.Contains(Target)) continue;
		FHitResultAndActor& Entry = Hits.AddDefaulted_GetRef();
		Entry.Result = *Hit;
		Entry.hitActor = Target;
		AddedActors.Add(Target);
	}
	return Hits;
}

void UCC_Primary::SendEventToActor(const TArray<FHitResultAndActor>& Hits)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(SourceASC))
	{
		return;
	}

	const AActor* Attacker = GetAvatarActorFromActorInfo();
	if (!IsValid(Attacker) || !Attacker->HasAuthority()) return;
	TSet<AActor*> SentActors;
	for (const FHitResultAndActor& Hit : Hits)
	{
		ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Hit.hitActor);
		// 蓝图可能传入自己组装的命中列表，这里再按 IsHostile 过滤一次，友方不受击。
		if (!IsValid(Character) || SentActors.Contains(Character) || !UCC_TeamStatics::IsHostile(Attacker, Character))
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


