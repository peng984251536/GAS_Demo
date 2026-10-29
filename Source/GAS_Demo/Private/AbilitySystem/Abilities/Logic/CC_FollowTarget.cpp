#include "AbilitySystem/Abilities/Logic/CC_FollowTarget.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AI/CC_CombatRangeStatics.h"
#include "AI/CC_MoveTaskWatchdog.h"
#include "Actors/Character/CC_BaseCharacter.h"
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
	// 允许重新出发：再次触发时 GAS 会先结束当前实例（EndAbility 清理旧移动任务）再重新激活。
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

	// 近战：直接追目标 Actor，走到攻击距离（边缘距离）即停。
	AActor* TargetActor = BaseCharacter->GetClosestActor().Actor.Get();
	if (IsValid(TargetActor) && !UCC_CombatRangeStatics::IsRangedCharacter(BaseCharacter))
	{
		AAIController* AIController = Cast<AAIController>(BaseCharacter->GetController());
		if (!IsValid(AIController))
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][Follow] %s: no AIController."), *GetNameSafe(BaseCharacter));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		// 停得比攻击距离再近一点，保证停下后攻击距离判断一定成立（见 MeleeApproachMargin）。
		const float MeleeReach = UCC_CombatRangeStatics::GetMeleeReach(BaseCharacter);
		const float StopDistance = FMath::Max(10.f, MeleeReach - MeleeApproachMargin);
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Follow] %s: chase %s (melee reach %.1f, stop %.1f)"),
			*GetNameSafe(BaseCharacter), *GetNameSafe(TargetActor), MeleeReach, StopDistance);
		StartMoveTask(AIController, FVector::ZeroVector, StopDistance, TargetActor);
		return;
	}

	AAIController* AIController = Cast<AAIController>(BaseCharacter->GetController());
	if (!IsValid(AIController))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][Follow] %s: no AIController."), *GetNameSafe(BaseCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 弓兵有目标：在目标当前位置周围找射程内的站位（不用 LastKnownLocation：那是锁定目标时的快照）。
	if (IsValid(TargetActor))
	{
		StartRangedApproach(*BaseCharacter, *TargetActor);
		return;
	}

	// 没有存活目标（目标离开视野）：直接走到最后看到它的位置搜索。
	const FVector knownLocation = BaseCharacter->GetClosestActor().LastKnownLocation;
	if (knownLocation == FVector::ZeroVector)
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Follow] %s: FollowTarget has no target and no last known location."),
			*GetNameSafe(BaseCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ShouldShowDebug())
	{
		const FString Message = FString::Printf(TEXT("[AI][Follow] %s: search -> %s"),
			*GetNameSafe(BaseCharacter), *knownLocation.ToString());
		UE_LOG(LogGAS_Demo, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Message);
		}
	}
	StartMoveTask(AIController, knownLocation, DefaultAcceptanceRadius);
}

void UCC_FollowTarget::StartRangedApproach(const ACC_BaseCharacter& Enemy, AActor& Target)
{
	AAIController* AIController = Cast<AAIController>(Enemy.GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Enemy.GetWorld());
	const UCC_CharacterConfig* Config = Enemy.GetCharacterConfig();
	if (!IsValid(AIController) || !IsValid(Nav) || !IsValid(Config))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Follow] %s: Cannot approach %s without data."),
			*GetNameSafe(&Enemy), *GetNameSafe(&Target));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float MinRange = FMath::Max(0.f, Config->MinRange);
	const float MaxRange = FMath::Max(MinRange + 1.f, Config->MaxRange);
	const float MidRange = (MinRange + MaxRange) * 0.5f;
	const FVector EnemyLocation = Enemy.GetActorLocation();
	const FVector TargetLocation = Target.GetActorLocation();

	// 从「自己相对目标的方向」开始找，尽量少绕路；重合时用自身朝向或世界 X 轴兜底。
	FVector Away = (EnemyLocation - TargetLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = Enemy.GetActorForwardVector().GetSafeNormal2D();
	}
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}

	const float CandidateRadii[] = {MidRange, (MinRange + MidRange) * 0.5f};
	const float CandidateAngles[] = {0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f};
	const FVector QueryExtent(150.f, 150.f, 800.f);
	for (const float Radius : CandidateRadii)
	{
		for (const float Angle : CandidateAngles)
		{
			const FVector Candidate = TargetLocation + Away.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
			FNavLocation OnNavMesh;
			if (!Nav->ProjectPointToNavigation(Candidate, OnNavMesh, QueryExtent))
			{
				continue;
			}
			// 投影可能把点挪近或挪远一些：只要仍在射程内就接受，不要求正好在中线上。
			const float Distance = FVector::Dist2D(OnNavMesh.Location, TargetLocation);
			if (Distance < MinRange || Distance > MaxRange)
			{
				continue;
			}
			const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
				Enemy.GetWorld(), EnemyLocation, OnNavMesh.Location);
			if (IsValid(Path) && Path->IsValid() && !Path->IsPartial())
			{
				UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Follow] %s: stand point %s (radius %.0f, angle %.0f)"),
					*GetNameSafe(&Enemy), *OnNavMesh.Location.ToCompactString(), Radius, Angle);
				StartMoveTask(AIController, OnNavMesh.Location, DefaultAcceptanceRadius);
				return;
			}
		}
	}

	// 一个站位都找不到：直接朝目标走到射程中线以内（边缘距离），进入射程后行为树接着射击。
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Follow] %s: no stand point around %s, approach directly (stop %.0f)."),
		*GetNameSafe(&Enemy), *GetNameSafe(&Target), MidRange);
	StartMoveTask(AIController, FVector::ZeroVector, MidRange, &Target);
}

void UCC_FollowTarget::StartMoveTask(AAIController* AIController, const FVector& Destination, float AcceptanceRadius,
	AActor* GoalActor)
{
	// 以 Actor 为终点时开启 StopOnOverlap：到达判定包含双方碰撞半径，与 UCC_CombatRangeStatics 的边缘距离一致。
	MoveTask = UAITask_MoveTo::AIMoveTo(AIController, Destination, GoalActor, AcceptanceRadius,
		GoalActor ? EAIOptionFlag::Enable : EAIOptionFlag::Default);
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

	// 可能已同步到达并结束；仍在移动时启动看门狗。
	if (IsActive() && IsValid(MoveTask) && GetWorld())
	{
		WatchedController = AIController;
		WatchedRequestId = CCMoveTaskWatchdog::GetCurrentRequestId(AIController);
		MoveStartTime = GetWorld()->GetTimeSeconds();
		GetWorld()->GetTimerManager().SetTimer(MoveWatchdogTimer, this, &ThisClass::CheckMoveWatchdog,
			CCMoveTaskWatchdog::CheckInterval, true);
	}
}

void UCC_FollowTarget::CheckMoveWatchdog()
{
	if (!IsActive() || !GetWorld())
	{
		return;
	}
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - MoveStartTime > MaxMoveDuration;
	const bool bOrphaned = CCMoveTaskWatchdog::IsMoveOrphaned(WatchedController.Get(), WatchedRequestId);
	if (bTimedOut || bOrphaned)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Follow] %s: move %s, ending FollowTarget."),
			*GetNameSafe(GetAvatarActorFromActorInfo()), bOrphaned ? TEXT("taken over / stopped") : TEXT("timed out"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bOrphaned);
	}
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
	if (ShouldShowDebug())
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveWatchdogTimer);
	}
	WatchedController.Reset();

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
