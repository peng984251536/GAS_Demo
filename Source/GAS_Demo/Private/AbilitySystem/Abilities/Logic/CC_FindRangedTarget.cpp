#include "AbilitySystem/Abilities/Logic/CC_FindRangedTarget.h"

#include "AbilitySystemComponent.h"
#include "Character/CC_EnemyCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "Engine/Engine.h"
#include "GAS_Demo.h"
#include "GameplayTags/CC_Tags.h"
#include "Utils/CC_BlueprintLibrary.h"

UCC_FindRangedTarget::UCC_FindRangedTarget()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRetriggerInstancedAbility = false;

	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::FindRangedTarget));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CCTags::CCAbilityTrigger::FindRangedTarget;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UCC_FindRangedTarget::PreActivate(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// 直接父类只初始化角色和组件，不要求战斗动作或蒙太奇。
	Super::PreActivate(Handle, ActorInfo, ActivationInfo,
		OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UCC_FindRangedTarget::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 本能力由原生代码完成激活，不调用父类的蓝图事件分发，避免重复执行蓝图查找/移动。
	// PreActivate 仍完整执行父类初始化；初始化失败后不能继续索敌。
	if (!IsActive())
	{
		return;
	}

	ACC_EnemyCharacter* Enemy = ActorInfo ? Cast<ACC_EnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Enemy) || !IsValid(ASC) || !ActorInfo->IsNetAuthority())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Find] Invalid activation: Enemy=%s ASC=%s Authority=%d"),
			*GetNameSafe(Enemy), *GetNameSafe(ASC), ActorInfo && ActorInfo->IsNetAuthority());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!Enemy->IsAlive() || ASC->HasMatchingGameplayTag(CCTags::Status::Death))
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Find] Enemy dead; clear target: %s"), *GetNameSafe(Enemy));
		Enemy->SetClosestActor(FClosestActorWithTagResult{});
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	// {
	// 	UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Find] CommitAbility failed: Enemy=%s"), *GetNameSafe(Enemy));
	// 	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	// 	return;
	// }

	// 提交能力的成本或冷却可能通过委托取消当前能力。
	if (!IsValid(Enemy))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UCC_CharacterConfig* CharacterConfig = static_cast<const ACC_BaseCharacter*>(Enemy)->GetCharacterConfig();
	if (!CharacterConfig)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Find] %s: Missing CharacterConfig."),
			*GetNameSafe(Enemy));
		Enemy->SetClosestActor(FClosestActorWithTagResult{});
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (CharacterConfig->MinRange < 0.0f ||
		CharacterConfig->MaxRange < CharacterConfig->MinRange)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Find] %s: Invalid range: Min=%.1f Max=%.1f."),
			*GetNameSafe(Enemy), CharacterConfig->MinRange, CharacterConfig->MaxRange);
		Enemy->SetClosestActor(FClosestActorWithTagResult{});
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FName TargetTag = CharacterConfig->TargetingActorTag.IsNone()
		? CrashTags::Player
		: CharacterConfig->TargetingActorTag;

	AActor* PreviousTarget = Enemy->GetClosestActor().Actor.Get();
	//const float PreviousDistance = Enemy->GetClosestActor().Distance;
	FClosestActorWithTagResult Target =
		UCC_BlueprintLibrary::FindClosestActorWithTag(
			Enemy, Enemy->GetActorLocation(), TargetTag);
	const float TargetDistance = FVector::Distance(Target.LastKnownLocation,BaseCharacter->GetActorLocation());
	
	// 找不到目标，或最近目标已超过索敌上限。
	if (!Target.Actor.IsValid() ||
		FVector::Distance(Target.Actor->GetActorLocation(),Enemy->GetActorLocation()) > CharacterConfig->MaxRange)
	{
		if (ShowDebug && IsValid(PreviousTarget))
		{
			UE_LOG(LogGAS_Demo, Log,
				TEXT("[BowAI][Find] Lost target: Enemy=%s Tag=%s Previous=%s Closest=%s Max=%.1f"),
				*GetNameSafe(Enemy), *TargetTag.ToString(), *GetNameSafe(PreviousTarget), *GetNameSafe(Target.Actor.Get()),
				CharacterConfig->MaxRange);
		}
		else if (ShowDebug)
		{
			UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Find] No target: Enemy=%s Tag=%s Closest=%s Distance=%.1f Max=%.1f"),
				*GetNameSafe(Enemy), *TargetTag.ToString(), *GetNameSafe(Target.Actor.Get()),
				TargetDistance,CharacterConfig->MaxRange);
		}
		Enemy->SetClosestActor(FClosestActorWithTagResult{});
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Enemy->SetClosestActor(Target);
	AActor* TargetActor = Target.Actor.Get();
	const float Distance = FVector::Distance(Target.LastKnownLocation,Enemy->GetActorLocation());
	if (ShowDebug && PreviousTarget != TargetActor)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Find] Target acquired: Enemy=%s Previous=%s Target=%s Distance=%.1f Min=%.1f Max=%.1f"),
			*GetNameSafe(Enemy), *GetNameSafe(PreviousTarget), *GetNameSafe(TargetActor), Distance,
			CharacterConfig->MinRange, CharacterConfig->MaxRange);
	}

	if (Distance >= CharacterConfig->MinRange)
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Find] In firing range: Enemy=%s Target=%s Distance=%.1f"),
			*GetNameSafe(Enemy), *GetNameSafe(TargetActor), Distance);
		FaceTargetAndEnd(*Enemy, *TargetActor, false);
		return;
	}
	// 索敌只记录当前目标并立即结束。贴身后的移动由 KeepDistance 独立负责，
	// 避免行为树重复索敌时同时创建两条 MoveTo 任务，互相取消导致原地打圈。
	if (ShowDebug && (PreviousTarget != TargetActor || Distance >= CharacterConfig->MinRange))
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Find] Too close; retreat needed: Enemy=%s Target=%s Distance=%.1f Min=%.1f"),
			*GetNameSafe(Enemy), *GetNameSafe(TargetActor), Distance, CharacterConfig->MinRange);
	}
	FaceTargetAndEnd(*Enemy, *TargetActor, false);
}

/**
 * 
 * @param Enemy 
 * @param TargetActor 
 * @param bWasCancelled 
 */
void UCC_FindRangedTarget::FaceTargetAndEnd(ACC_EnemyCharacter& Enemy, const AActor& TargetActor, bool bWasCancelled)
{
	const FVector findVector = TargetActor.GetActorLocation() - Enemy.GetActorLocation();
	
	// 蒙太奇里的转身通知会读取此方向；索敌时写入一次，放箭时再按移动后的目标位置更新。
	Enemy.SetLastMoveInputDirection(findVector);
	
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Find] Facing saved: Enemy=%s Target=%s Direction=%s Cancelled=%d"),
		*GetNameSafe(&Enemy), *GetNameSafe(&TargetActor), *findVector.ToCompactString(), bWasCancelled);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
