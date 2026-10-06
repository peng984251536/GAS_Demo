#include "AbilitySystem/Abilities/Logic/CC_FindPlayerTarget.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Character/CC_EnemyCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "Engine/Engine.h"
#include "GAS_Demo.h"
#include "GameplayTags/CC_Tags.h"
#include "Navigation/PathFollowingComponent.h"
#include "Tasks/AITask_MoveTo.h"
#include "Utils/CC_BlueprintLibrary.h"

UCC_FindPlayerTarget::UCC_FindPlayerTarget()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = false;

	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::FindPlayerTarget));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CCTags::CCAbilityTrigger::FindPlayerTarget;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UCC_FindPlayerTarget::PreActivate(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// 直接父类只初始化角色和组件，不要求战斗动作或蒙太奇。
	Super::PreActivate(Handle, ActorInfo, ActivationInfo,
		OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UCC_FindPlayerTarget::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 本能力由原生代码完成激活，不调用父类的蓝图事件分发，避免重复执行蓝图查找/移动。
	// PreActivate 仍完整执行父类初始化；初始化失败后不能继续追赶。
	if (!IsActive())
	{
		return;
	}
	
	UAbilitySystemComponent* ASC = BaseCharacter->GetAbilitySystemComponent();
	if (!IsValid(BaseCharacter) || !IsValid(ASC) || !ActorInfo->IsNetAuthority())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("FindPlayerTarget requires an enemy avatar and an authoritative ASC."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!BaseCharacter->IsAlive() || ASC->HasMatchingGameplayTag(CCTags::Status::Death))
	{
		BaseCharacter->SetClosestActor(FClosestActorWithTagResult{});
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	// {
	// 	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	// 	return;
	// }
	// 提交能力的成本或冷却可能通过委托取消当前能力。
	// if (!IsValid(BaseCharacter))
	// {
	// 	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	// 	return;
	// }

	// 索敌目标 Tag 由角色配置决定；配置缺失或为空时回退到 Player，保持旧行为。
	// Enemy 的 GetCharacterConfig 重写是 protected，经由公开基类接口访问。
	const UCC_CharacterConfig* CharacterConfig = BaseCharacter->GetCharacterConfig();
	// const FName TargetTag = (CharacterConfig && !CharacterConfig->TargetingActorTag.IsNone())
	// 	? CharacterConfig->TargetingActorTag : CrashTags::Player;
	
	const FClosestActorWithTagResult Target = BaseCharacter->GetClosestActor();
	if (!Target.Actor.IsValid())
	{
		//Target = FClosestActorWithTagResult{};
		UE_LOG(LogGAS_Demo, Log, TEXT("FClosestActorWithTagResult is nullptr"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	AActor* TargetActor = Target.Actor.Get();
	//
	// const FVector EnemyLocation = BaseCharacter->GetActorLocation();
	// const FVector TargetLocation = TargetActor->GetActorLocation();
	// const float Distance = FVector::Dist2D(EnemyLocation, TargetLocation);
	// if(Distance > BaseCharacter->GetCharacterConfig()->MaxRange)
	// {
	// 	Target = FClosestActorWithTagResult{};
	// 	UE_LOG(LogGAS_Demo, Log, TEXT("FClosestActorWithTagResult is nullptr"));
	// 	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	// 	return;
	// }
	//
	// //BaseCharacter->SetClosestActor(Target);
	// const float TargetDistance = FVector::Distance(Target.LastKnownLocation,BaseCharacter->GetActorLocation());
	// if (ShowDebug)
	// {
	// 	const FString Message = FString::Printf(TEXT("%s: FindPlayerTarget -> %s (Distance: %.1f)"),
	// 		*GetNameSafe(BaseCharacter), *GetNameSafe(Target.Actor.Get()), TargetDistance);
	// 	UE_LOG(LogGAS_Demo, Log, TEXT("%s"), *Message);
	// 	if (GEngine)
	// 	{
	// 		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Message);
	// 	}
	// }

	StartMoveToTarget(*BaseCharacter, *TargetActor);
}

/**
 * 移动到某对象
 * @param Enemy 
 * @param TargetActor 
 */
void UCC_FindPlayerTarget::StartMoveToTarget(ACC_BaseCharacter& Enemy, AActor& TargetActor)
{
	
	AAIController* AIController = Cast<AAIController>(Enemy.GetController());
	if (!IsValid(AIController))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Cannot pursue %s without an AIController."),
			*GetNameSafe(&Enemy), *GetNameSafe(&TargetActor));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	
	MoveTask = UAITask_MoveTo::AIMoveTo(
		AIController, FVector::ZeroVector, &TargetActor, Enemy.GetCharacterConfig()->MinRange);
	if (!IsValid(MoveTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 已经到达或请求无效时，ReadyForActivation 可能同步触发回调，因此先绑定。
	// 此原生委托同时覆盖蓝图的 OnMoveFinished 和 OnRequestFailed。
	MoveTask->OnMoveTaskFinished.AddUObject(this, &ThisClass::HandleMoveFinished);
	MoveTask->ReadyForActivation();
}

void UCC_FindPlayerTarget::HandleMoveFinished(
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
		const FString Message = FString::Printf(TEXT("%s: MoveToTarget finished: %s"),
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
		ACC_EnemyCharacter* Enemy = Cast<ACC_EnemyCharacter>(GetAvatarActorFromActorInfo());
		if (IsValid(Enemy))
		{
			const AActor* TargetActor = Enemy->GetClosestActor().Actor.Get();
			if (IsValid(TargetActor))
			{
				// 到达后用双方当前位置计算攻击方向，确保能力结束回调读到最新值。
				Enemy->SetLastMoveInputDirection(TargetActor->GetActorLocation() - Enemy->GetActorLocation());
			}
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bSucceeded);
}

void UCC_FindPlayerTarget::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
		// 释放 AI 资源可能触发行为树回调，清理期间禁止重复结束同一个能力。
		TGuardValue<bool> EndingGuard(bIsAbilityEnding, true);
		UAITask_MoveTo* TaskToEnd = MoveTask;
		MoveTask = nullptr;
		if (IsValid(TaskToEnd))
		{
			TaskToEnd->OnMoveTaskFinished.RemoveAll(this);
			// 只取消此任务自己的移动请求，同时释放 AI 逻辑锁。
			TaskToEnd->EndTask();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
