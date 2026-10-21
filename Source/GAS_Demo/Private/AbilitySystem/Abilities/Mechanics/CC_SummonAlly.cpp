#include "AbilitySystem/Abilities/Mechanics/CC_SummonAlly.h"

#include "GAS_Demo.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/Targeting/CC_GroundPlacementIndicator.h"
#include "AbilitySystem/Targeting/CC_TargetActor_GroundPlacement.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameplayTags/CC_Tags.h"
#include "NavigationSystem.h"
#include "Utils/Team/CC_TeamStatics.h"

namespace
{
/** 生成时胶囊底部离地的间隙（厘米）。 */
constexpr float SpawnGroundClearance = 2.f;
}

UCC_SummonAlly::UCC_SummonAlly()
{
	// 每个施放者一个实例：随从列表跟着实例走，不放在 CDO 上。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 本地预测：瞄准和确认在客户端即时响应，落点通过 TargetData 发给服务器。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilities::Summon));
	ActivationOwnedTags.AddTag(CCTags::Status::Targeting);
	ActivationBlockedTags.AddTag(CCTags::Status::Death);
	ActivationBlockedTags.AddTag(CCTags::Status::BeHit);
	ActivationBlockedTags.AddTag(CCTags::Status::Targeting);
	ActivationBlockedTags.AddTag(CCTags::CCAbilities::Attack);

	TargetActorClass = ACC_TargetActor_GroundPlacement::StaticClass();
	IndicatorClass = ACC_GroundPlacementIndicator::StaticClass();
}

void UCC_SummonAlly::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	const bool bHasController = ActorInfo && ActorInfo->PlayerController.IsValid();
	if (!IsValid(BaseCharacter) || !BaseCharacter->IsAlive() || !SummonClass
		|| SummonClass->HasAnyClassFlags(CLASS_Abstract) || !TargetActorClass || !bHasController)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] Activation rejected Avatar=%s SummonClass=%s HasPC=%d"),
			*GetNameSafe(BaseCharacter), *GetNameSafe(SummonClass.Get()), bHasController);
		CancelActivation();
		return;
	}

	TargetTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("SummonPlacement"),
		EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
	if (!TargetTask)
	{
		CancelActivation();
		return;
	}
	TargetTask->ValidData.AddDynamic(this, &ThisClass::OnPlacementConfirmed);
	TargetTask->Cancelled.AddDynamic(this, &ThisClass::OnPlacementCancelled);

	// 只在本地控制端真正生成 Target Actor；远程客户端的服务器实例直接等待复制来的 TargetData。
	AGameplayAbilityTargetActor* SpawnedActor = nullptr;
	if (TargetTask->BeginSpawningActor(this, TargetActorClass, SpawnedActor) && SpawnedActor)
	{
		ConfigureTargetActor(Cast<ACC_TargetActor_GroundPlacement>(SpawnedActor));
		TargetTask->FinishSpawningActor(this, SpawnedActor);
	}
	TargetTask->ReadyForActivation();
}

void UCC_SummonAlly::ConfigureTargetActor(ACC_TargetActor_GroundPlacement* TargetActor) const
{
	if (!TargetActor)
	{
		return;
	}
	TargetActor->MaxRange = MaxRange;
	TargetActor->IndicatorClass = IndicatorClass;
	TargetActor->IndicatorRadius = IndicatorRadius;

	const ACC_EnemyCharacter* SummonCDO = SummonClass ? SummonClass->GetDefaultObject<ACC_EnemyCharacter>() : nullptr;
	if (const UCapsuleComponent* Capsule = SummonCDO ? SummonCDO->GetCapsuleComponent() : nullptr)
	{
		TargetActor->CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		TargetActor->CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
}

void UCC_SummonAlly::OnPlacementConfirmed(const FGameplayAbilityTargetDataHandle& Data)
{
	const FGameplayAbilityTargetData* Target = Data.Get(0);
	if (!Target || !Target->HasEndPoint())
	{
		CancelActivation();
		return;
	}
	// 确认时才提交冷却 / 消耗：取消选点不扣。
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		CancelActivation();
		return;
	}
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		SpawnSummon(BaseCharacter, Target->GetEndPoint());
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCC_SummonAlly::OnPlacementCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	CancelActivation();
}

void UCC_SummonAlly::CancelActivation()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

ACC_EnemyCharacter* UCC_SummonAlly::SpawnSummon(ACC_BaseCharacter* Caster, const FVector& RequestedLocation)
{
	UWorld* World = IsValid(Caster) ? Caster->GetWorld() : nullptr;
	if (!World || !Caster->HasAuthority() || !SummonClass || SummonClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	// 服务器重新校验：客户端提交的落点不可信。
	const float Distance = FVector::Dist2D(Caster->GetActorLocation(), RequestedLocation);
	if (MaxRange > 0.f && Distance > MaxRange + RangeTolerance)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] Rejected: out of range Caster=%s Distance=%.0f Max=%.0f"),
			*GetNameSafe(Caster), Distance, MaxRange);
		return nullptr;
	}

	FVector Location = RequestedLocation;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		if (Nav->GetDefaultNavDataInstance())
		{
			FNavLocation NavLocation;
			if (!Nav->ProjectPointToNavigation(RequestedLocation, NavLocation, NavProjectExtent))
			{
				UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] Rejected: not on navmesh Caster=%s Location=%s"),
					*GetNameSafe(Caster), *RequestedLocation.ToCompactString());
				return nullptr;
			}
			Location = NavLocation.Location;
		}
	}

	const ACC_EnemyCharacter* SummonCDO = SummonClass->GetDefaultObject<ACC_EnemyCharacter>();
	const UCapsuleComponent* CDOCapsule = SummonCDO ? SummonCDO->GetCapsuleComponent() : nullptr;
	Location.Z += (CDOCapsule ? CDOCapsule->GetScaledCapsuleHalfHeight() : 88.f) + SpawnGroundClearance;

	// 面朝施放者指向落点的方向。
	FVector Facing = Location - Caster->GetActorLocation();
	Facing.Z = 0.f;
	const float Yaw = Facing.IsNearlyZero() ? Caster->GetActorRotation().Yaw : Facing.Rotation().Yaw;
	const FTransform SpawnTransform(FRotator(0.f, Yaw, 0.f), Location);

	ACC_EnemyCharacter* Summon = World->SpawnActorDeferred<ACC_EnemyCharacter>(SummonClass, SpawnTransform, Caster, Caster,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Summon)
	{
		return nullptr;
	}
	// 必须在 FinishSpawning 前写入：BeginPlay 时 AI 感知、选目标就已经按新阵营工作。
	Summon->SetTeamOverride(UCC_TeamStatics::GetBaseTeam(Caster));
	if (bOverrideSpawnParams)
	{
		Summon->SetSpawnParams(SummonSpawnParams);
	}
	Summon->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Summon->FinishSpawning(SpawnTransform);
	if (!IsValid(Summon))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] Spawn blocked at %s"), *Location.ToCompactString());
		return nullptr;
	}

	if (SummonLifeSpan > 0.f)
	{
		Summon->SetLifeSpan(SummonLifeSpan);
	}
	ActiveSummons.Add(Summon);
	EnforceSummonLimit();

	UE_LOG(LogGAS_Demo, Log, TEXT("[Summon] Caster=%s Summon=%s Team=%s Location=%s Active=%d"),
		*GetNameSafe(Caster), *GetNameSafe(Summon), *UCC_TeamStatics::GetBaseTeam(Summon).ToString(),
		*Location.ToCompactString(), ActiveSummons.Num());
	return Summon;
}

void UCC_SummonAlly::EnforceSummonLimit()
{
	ActiveSummons.RemoveAll([](const TWeakObjectPtr<ACC_EnemyCharacter>& Summon)
	{
		return !Summon.IsValid() || !Summon->IsAlive();
	});
	if (MaxActiveSummons <= 0)
	{
		return;
	}
	while (ActiveSummons.Num() > MaxActiveSummons)
	{
		ACC_EnemyCharacter* Oldest = ActiveSummons[0].Get();
		ActiveSummons.RemoveAt(0);
		if (IsValid(Oldest))
		{
			Oldest->Destroy();
		}
	}
}

void UCC_SummonAlly::DestroyAllSummons()
{
	TArray<TWeakObjectPtr<ACC_EnemyCharacter>> Summons = MoveTemp(ActiveSummons);
	ActiveSummons.Reset();
	for (const TWeakObjectPtr<ACC_EnemyCharacter>& Summon : Summons)
	{
		if (Summon.IsValid())
		{
			Summon->Destroy();
		}
	}
}

TArray<ACC_EnemyCharacter*> UCC_SummonAlly::GetActiveSummons() const
{
	TArray<ACC_EnemyCharacter*> Result;
	for (const TWeakObjectPtr<ACC_EnemyCharacter>& Summon : ActiveSummons)
	{
		if (Summon.IsValid() && Summon->IsAlive())
		{
			Result.Add(Summon.Get());
		}
	}
	return Result;
}

void UCC_SummonAlly::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}
	// 父类结束全部 AbilityTask；WaitTargetData 结束时销毁 Target Actor，指示器和鼠标状态随之回收。
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	TargetTask = nullptr;
}

void UCC_SummonAlly::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (bDestroySummonsOnAbilityRemoved && ActorInfo && ActorInfo->IsNetAuthority())
	{
		DestroyAllSummons();
	}
	Super::OnRemoveAbility(ActorInfo, Spec);
}
