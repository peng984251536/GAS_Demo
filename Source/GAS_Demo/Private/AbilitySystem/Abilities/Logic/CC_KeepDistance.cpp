#include "AbilitySystem/Abilities/Logic/CC_KeepDistance.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Character/CC_EnemyCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "GAS_Demo.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Tasks/AITask_MoveTo.h"

UCC_KeepDistance::UCC_KeepDistance()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;
	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::KeepDistance));
	// 放箭动作期间不抢走 AI 移动控制权；攻击结束后由下次检查决定是否退。
	ActivationBlockedTags.AddTag(CCTags::CCAbilities::Attack);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CCTags::CCAbilityTrigger::KeepDistance;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UCC_KeepDistance::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!IsActive())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Retreat] Activation is no longer active: Avatar=%s"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
		return;
	}
	
	ACC_EnemyCharacter* Enemy = ActorInfo ? Cast<ACC_EnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	
	const UCC_CharacterConfig* Config = IsValid(Enemy) ? Enemy->GetCharacterConfig() : nullptr;

	
	AActor* Target = IsValid(Enemy) ? Enemy->GetClosestActor().Actor.Get() : nullptr;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !IsValid(Enemy) || !Enemy->IsAlive()
		|| !IsValid(ASC) || ASC->HasMatchingGameplayTag(CCTags::Status::Death)
		|| !IsValid(Config) || !IsValid(Target) || Config->MinRange <= 0.0f)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Retreat] Invalid activation: Enemy=%s Target=%s Config=%s ASC=%s Authority=%d"),
			*GetNameSafe(Enemy), *GetNameSafe(Target), *GetNameSafe(Config), *GetNameSafe(ASC),
			ActorInfo && ActorInfo->IsNetAuthority());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	
	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(EnemyLocation, TargetLocation);
	if (Distance >= Config->MinRange || Distance > Config->MaxRange)
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Retreat] No retreat needed: Enemy=%s Target=%s Distance=%.1f Min=%.1f Max=%.1f"),
			*GetNameSafe(Enemy), *GetNameSafe(Target), Distance, Config->MinRange, Config->MaxRange);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AAIController* Controller = Cast<AAIController>(Enemy->GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Enemy->GetWorld());
	if (!IsValid(Controller) || !IsValid(Nav))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Retreat] Missing AIController or NavSystem: Enemy=%s Controller=%s Nav=%s"),
			*GetNameSafe(Enemy), *GetNameSafe(Controller), *GetNameSafe(Nav));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Away = (EnemyLocation - TargetLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}
	// 目标完全重合时也给导航一个稳定的方向；角度候选能绕开身后的静态障碍。
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	const float MidRange = (Config->MinRange + Config->MaxRange)*0.5f;
	const float DesiredDistance = FMath::Min(Config->MaxRange, MidRange);
	//const float CandidateAngles[] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f};
	const float CandidateAngles[] = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f};
	FVector Destination = FVector::ZeroVector;
	bool bFoundDestination = false;
	for (const float Angle : CandidateAngles)
	{
		const FVector Candidate = TargetLocation + Away.RotateAngleAxis(Angle, FVector::UpVector) * DesiredDistance;
		// 到时候范围的地点信息
		FNavLocation OnNavMesh;
		// 投影的信息
		const FVector QueryExtent(150.0f, 150.0f, 800.0f);
		if (!Nav->ProjectPointToNavigation(Candidate, OnNavMesh,QueryExtent)
			|| FVector::Dist2D(OnNavMesh.Location, TargetLocation) < MidRange)
		{
			UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Retreat] Candidate rejected by nav/range: Enemy=%s Angle=%.0f Candidate=%s"),
				*GetNameSafe(Enemy), Angle, *Candidate.ToCompactString());
			continue;
		}
		// 之前的是两套导航系统
		const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
			Enemy->GetWorld(), EnemyLocation, OnNavMesh.Location,Enemy);
		// const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
		// 	Enemy->GetWorld(), EnemyLocation, OnNavMesh.Location);
		if (IsValid(Path) && Path->IsValid() && !Path->IsPartial() &&
			Path->GetPathLength() > KINDA_SMALL_NUMBER )
		{
			Destination = OnNavMesh.Location;
			bFoundDestination = true;
			break;
		}
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Retreat] Candidate has no complete path: Enemy=%s Angle=%.0f Destination=%s"),
			*GetNameSafe(Enemy), Angle, *OnNavMesh.Location.ToCompactString());
	}
	if (!bFoundDestination)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Retreat] No reachable destination: Enemy=%s Target=%s Distance=%.1f Min=%.1f"),
			*GetNameSafe(Enemy), *GetNameSafe(Target), Distance, Config->MinRange);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// {
	// 	FVector dir = Enemy->GetActorLocation() - Target->GetActorLocation();
	// 	dir.Normalize();
	// 	Destination = dir*50;
	// }   log LogGAS_Demo Verbose
	
	
	MoveDestination = Destination;
	UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
	UAITask_MoveTo* CreatedTask = UAITask_MoveTo::AIMoveTo(Controller, Destination, nullptr, 10.0f);
	//UAITask_MoveTo* CreatedTask = UAITask_MoveTo::AIMoveTo(Controller, FVector::ZeroVector, Target, 10.0f);
	MoveTask = CreatedTask;

	
	// 同时记录原始返回值与成员变量，区分创建失败、赋值后失效和没有 GameplayTasks 组件。
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[BowAI][Retreat] Task created: Enemy=%s Controller=%s Pawn=%s TasksComponent=%s PathFollowing=%s RawTask=%p StoredTask=%p RawValid=%d StoredValid=%d"),
		*GetNameSafe(Enemy), *GetNameSafe(Controller), *GetNameSafe(Controller->GetPawn()),
		*GetNameSafe(Controller->GetGameplayTasksComponent()), *GetNameSafe(PathFollowing),
		static_cast<void*>(CreatedTask), static_cast<void*>(MoveTask.Get()),
		IsValid(CreatedTask), IsValid(MoveTask));

	
	if (!IsValid(MoveTask))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Retreat] AIMoveTo task creation failed: Enemy=%s Controller=%s Pawn=%s TasksComponent=%s PathFollowing=%s RawTask=%p RawValid=%d Destination=%s"),
			*GetNameSafe(Enemy), *GetNameSafe(Controller), *GetNameSafe(Controller->GetPawn()),
			*GetNameSafe(Controller->GetGameplayTasksComponent()), *GetNameSafe(PathFollowing),
			static_cast<void*>(CreatedTask), IsValid(CreatedTask), *Destination.ToCompactString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 必须在 ReadyForActivation 之前订阅：移动请求可能同步失败或立即到达。
	if (IsValid(PathFollowing))
	{
		ObservedPathFollowing = PathFollowing;
		PathRequestFinishedHandle = PathFollowing->OnRequestFinished.AddUObject(
			this, &ThisClass::HandlePathRequestFinished);
	}
	// AI Task 可能立即完成，回调必须先于 ReadyForActivation 绑定。
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Retreat] Move started: Enemy=%s Target=%s Distance=%.1f Destination=%s DesiredDistance=%.1f"),
		*GetNameSafe(Enemy), *GetNameSafe(Target), Distance, *Destination.ToCompactString(), DesiredDistance);
	
	MoveTask->OnMoveTaskFinished.AddUObject(this, &ThisClass::HandleMoveFinished);
	// UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Retreat] Task activating: Enemy=%s Task=%s State=%d PathStatus=%d Request=%u"),
	// 	*GetNameSafe(Enemy), *GetNameSafe(MoveTask), static_cast<int32>(MoveTask->GetState()),
	// 	IsValid(PathFollowing) ? static_cast<int32>(PathFollowing->GetStatus()) : -1,
	// 	IsValid(PathFollowing) ? PathFollowing->GetCurrentRequestId().GetID() : 0u);

	
	MoveTask->ReadyForActivation();
	
	// if (IsActive() && IsValid(MoveTask) && GetWorld())
	// {
	// 	// 半秒采样一次位置、速度和任务状态；使用 Verbose 避免常规日志刷屏。
	// 	GetWorld()->GetTimerManager().SetTimer(MoveProgressTimer, this, &ThisClass::LogMoveProgress, 0.5f, true);
	// 	LogMoveProgress();
	// }
}

void UCC_KeepDistance::HandlePathRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[BowAI][Retreat] Path finished: Enemy=%s Request=%u Result=%s Flags=0x%04X NewRequest=%d OwnerFinished=%d InvalidPath=%d MovementStop=%d ForcedScript=%d Task=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), RequestID.GetID(), *Result.ToString(),
		static_cast<uint32>(Result.Flags),
		Result.HasFlag(FPathFollowingResultFlags::NewRequest),
		Result.HasFlag(FPathFollowingResultFlags::OwnerFinished),
		Result.HasFlag(FPathFollowingResultFlags::InvalidPath),
		Result.HasFlag(FPathFollowingResultFlags::MovementStop),
		Result.HasFlag(FPathFollowingResultFlags::ForcedScript), *GetNameSafe(MoveTask));
}

void UCC_KeepDistance::LogMoveProgress()
{
	if (!IsActive() || !IsValid(MoveTask))
	{
		StopMoveDiagnostics();
		return;
	}
	const ACC_EnemyCharacter* Enemy = Cast<ACC_EnemyCharacter>(GetAvatarActorFromActorInfo());
	const AAIController* Controller = IsValid(Enemy) ? Cast<AAIController>(Enemy->GetController()) : nullptr;
	const UPathFollowingComponent* PathFollowing = IsValid(Controller) ? Controller->GetPathFollowingComponent() : nullptr;
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[BowAI][Retreat] Progress: Enemy=%s Task=%s State=%d PathStatus=%d Request=%u DistanceToGoal=%.1f Velocity=%s"),
		*GetNameSafe(Enemy), *GetNameSafe(MoveTask), static_cast<int32>(MoveTask->GetState()),
		IsValid(PathFollowing) ? static_cast<int32>(PathFollowing->GetStatus()) : -1,
		IsValid(PathFollowing) ? PathFollowing->GetCurrentRequestId().GetID() : 0u,
		IsValid(Enemy) ? FVector::Dist2D(Enemy->GetActorLocation(), MoveDestination) : -1.0f,
		IsValid(Enemy) ? *Enemy->GetVelocity().ToCompactString() : TEXT("None"));
}

void UCC_KeepDistance::StopMoveDiagnostics()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MoveProgressTimer);
	}
	if (UPathFollowingComponent* PathFollowing = ObservedPathFollowing.Get())
	{
		PathFollowing->OnRequestFinished.Remove(PathRequestFinishedHandle);
	}
	ObservedPathFollowing.Reset();
	PathRequestFinishedHandle.Reset();
}

void UCC_KeepDistance::HandleMoveFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* Controller)
{
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Retreat] Move finished: Enemy=%s Controller=%s Result=%d Active=%d Task=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Controller), static_cast<int32>(Result.GetValue()),
		IsActive(), *GetNameSafe(MoveTask));
	MoveTask = nullptr;
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			true, Result != EPathFollowingResult::Success);
	}
}

void UCC_KeepDistance::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Retreat] End: Enemy=%s Cancelled=%d MoveTask=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), bWasCancelled, *GetNameSafe(MoveTask));
	StopMoveDiagnostics();
	if (IsValid(MoveTask))
	{
		UAITask_MoveTo* Task = MoveTask;
		MoveTask = nullptr;
		Task->OnMoveTaskFinished.RemoveAll(this);
		Task->EndTask();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
