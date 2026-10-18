#include "AbilitySystem/Abilities/Logic/CC_FollowTarget.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AI/Engagement/CC_EngagementSubsystem.h"
#include "Character/CC_BaseCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS_Demo.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "Tasks/AITask_MoveTo.h"

UCC_FollowTarget::UCC_FollowTarget()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	// 分配位置变化后需要重新出发：再次触发时 GAS 会先结束当前实例（EndAbility 清理旧移动任务）再重新激活。
	bRetriggerInstancedAbility = true;

	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::FollowTarget));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CCTags::CCAbilityTrigger::FollowTarget;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}


void UCC_FollowTarget::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 本能力由原生代码完成激活，不调用父类的蓝图事件分发。
	// PreActivate 仍完整执行父类初始化；初始化失败后不能继续。
	if (!IsActive())
	{
		return;
	}

	// 优先走到围攻子系统分配的位置。
	FVector Destination;
	float AcceptanceRadius = DefaultAcceptanceRadius;
	if (TryGetEngagementDestination(Destination, AcceptanceRadius))
	{
		AAIController* AIController = Cast<AAIController>(BaseCharacter->GetController());
		if (!IsValid(AIController))
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][Follow] %s: no AIController."), *GetNameSafe(BaseCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Follow] %s: move to assignment %s (acceptance %.1f)"),
			*GetNameSafe(BaseCharacter), *Destination.ToCompactString(), AcceptanceRadius);
		StartMoveTask(AIController, Destination, AcceptanceRadius);
		return;
	}

	// 回退：没有围攻分配（例如子系统不存在或尚未注册），沿用旧的环绕选点逻辑。
	const FVector knownLocation = BaseCharacter->GetClosestActor().LastKnownLocation;
	if (knownLocation == FVector::ZeroVector)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Follow] %s: FollowTarget has no valid target."),
			*GetNameSafe(BaseCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ShowDebug)
	{
		const FString Message = FString::Printf(
			TEXT("[BowAI][Follow] %s: FollowTarget -> %s"),
			*GetNameSafe(BaseCharacter),
			*knownLocation.ToString());
		UE_LOG(LogGAS_Demo, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Message);
		}
	}

	StartMoveToTarget(*BaseCharacter,knownLocation);
}

bool UCC_FollowTarget::TryGetEngagementDestination(FVector& OutDestination, float& OutAcceptanceRadius) const
{
	const UWorld* World = GetWorld();
	const UCC_EngagementSubsystem* Subsystem = World ? World->GetSubsystem<UCC_EngagementSubsystem>() : nullptr;
	if (!Subsystem || !IsValid(BaseCharacter))
	{
		return false;
	}
	const FCC_EngagementAssignment Assignment = Subsystem->GetAssignment(BaseCharacter);
	if (Assignment.Tier == ECC_EngagementTier::None || !Assignment.bLocationValid)
	{
		return false;
	}
	OutDestination = Assignment.MoveLocation;
	OutAcceptanceRadius = Assignment.Tier == ECC_EngagementTier::Engaged
		? EngagedAcceptanceRadius
		: DefaultAcceptanceRadius;
	return true;
}

/**
 * 回退逻辑：围绕已知位置按角度候选选出可达点。
 * @param Enemy
 * @param knownLocation
 */
void UCC_FollowTarget::StartMoveToTarget(const ACC_BaseCharacter& Enemy, const FVector& TargetLocation)
{

	AAIController* AIController = Cast<AAIController>(Enemy.GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Enemy.GetWorld());
	const UCC_CharacterConfig* Config = Enemy.GetCharacterConfig();
	if (!IsValid(AIController)|| !IsValid(Nav) || !IsValid(Config))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Follow] %s: Cannot pursue %s without data."),
			*GetNameSafe(&Enemy), *TargetLocation.ToString());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FVector EnemyLocation = Enemy.GetActorLocation();
	const float MidRange = (Config->MinRange + Config->MaxRange)*0.5f;
	// 目标点应位于攻击范围中线附近；用范围宽度的一半会在很多配置下小于
	// MidRange，导致所有候选点都被下面的距离检查过滤掉，能力立即结束并被行为树反复触发。
	const float DesiredDistance = FMath::Max(MidRange, 50.0f);
	//const float CandidateAngles[] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f};
	const float CandidateAngles[] = {0.0f, 90.0f, 180.0f, 270.0f};
	FVector Destination = FVector::ZeroVector;
	bool bFoundDestination = false;
	FVector Away = (EnemyLocation - TargetLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = Enemy.GetActorForwardVector().GetSafeNormal2D();
	}
	// 目标完全重合时也给导航一个稳定的方向；角度候选能绕开身后的静态障碍。
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}

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
			continue;
		}
		// 之前的是两套导航系统
		const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
			Enemy.GetWorld(), EnemyLocation, OnNavMesh.Location);
		// const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
		// 	Enemy->GetWorld(), EnemyLocation, OnNavMesh.Location);
		if (IsValid(Path) && Path->IsValid() && !Path->IsPartial() &&
			Path->GetPathLength() > KINDA_SMALL_NUMBER )
		{
			Destination = OnNavMesh.Location;
			bFoundDestination = true;
			break;
		}
	}
	if (!bFoundDestination)
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Follow] %s: no reachable destination near %s (desired %.1f, mid %.1f)."),
			*GetNameSafe(&Enemy), *TargetLocation.ToString(), DesiredDistance, MidRange);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	StartMoveTask(AIController, Destination, DefaultAcceptanceRadius);
}

void UCC_FollowTarget::StartMoveTask(AAIController* AIController, const FVector& Destination, float AcceptanceRadius)
{
	MoveTask = UAITask_MoveTo::AIMoveTo(AIController, Destination, nullptr, AcceptanceRadius);
	if (!IsValid(MoveTask))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Follow] MoveTask nullptr"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 已经到达或请求无效时，ReadyForActivation 可能同步触发回调，因此先绑定。
	// 此原生委托同时覆盖蓝图的 OnMoveFinished 和 OnRequestFailed。
	MoveTask->OnMoveTaskFinished.AddUObject(this, &ThisClass::HandleMoveFinished);
	MoveTask->ReadyForActivation();
}

void UCC_FollowTarget::HandleMoveFinished(
	TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* AIController)
{
	// UAITask_MoveTo 在广播前已经结束任务，这里只结束对应的能力。
	MoveTask = nullptr;
	if (!IsActive())
	{
		return;
	}

	const bool bSucceeded = Result == EPathFollowingResult::Success;
	if (ShowDebug)
	{
		const FString Message = FString::Printf(TEXT("[BowAI][Follow] %s: MoveToTarget finished: %s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*UEnum::GetValueAsString(Result.GetValue()));
		UE_LOG(LogGAS_Demo, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, bSucceeded ? FColor::Green : FColor::Red, Message);
		}
	}
	if (bSucceeded)
	{
		const AActor* TargetActor = BaseCharacter->GetClosestActor().Actor.Get();
		if (IsValid(TargetActor))
		{
			// 到达后用双方当前位置计算攻击方向，确保能力结束回调读到最新值。
			BaseCharacter->SetLastMoveInputDirection(
				TargetActor->GetActorLocation() - BaseCharacter->GetActorLocation());
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bSucceeded);
}

void UCC_FollowTarget::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	{
		// 重新触发或被打断时，旧移动任务必须先解绑再结束：
		// 否则它被新的移动请求顶掉时会回调到新激活的实例，把新实例也结束掉。
		TGuardValue<bool> EndingGuard(bIsAbilityEnding, true);
		UAITask_MoveTo* TaskToEnd = MoveTask;
		MoveTask = nullptr;
		if (IsValid(TaskToEnd))
		{
			TaskToEnd->OnMoveTaskFinished.RemoveAll(this);
			TaskToEnd->EndTask();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
