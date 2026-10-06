#include "AbilitySystem/Abilities/Logic/CC_FollowTarget.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Character/CC_BaseCharacter.h"
#include "Engine/Engine.h"
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
	bRetriggerInstancedAbility = false;

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

	// 跟随逻辑由你补全（参考 CC_FindPlayerTarget 的 UAITask_MoveTo 用法）。
	StartMoveToTarget(*BaseCharacter,knownLocation);
}

/**
 * 移动到某对象
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
	
	if (!IsValid(AIController))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Follow] %s: Cannot pursue %s without an AIController."),
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

	
	MoveTask = UAITask_MoveTo::AIMoveTo(
		AIController, Destination, nullptr, 50.0f);


	
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

