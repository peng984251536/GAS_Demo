// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Reaction/CC_HitReact.h"
#include "GAS_Demo.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Utils/CC_BlueprintLibrary.h"


/**
 * 暂停/恢复 avatar 所属 AI 的行为树逻辑（PauseLogic/ResumeLogic，保留行为树状态）。
 * 只对 AAIController 生效：玩家（PlayerController）安全空转；未运行行为树（无 BrainComponent）安全空转。
 * 返回是否实际操作了 BrainComponent。public static 以便脱离激活链路单测。
 */
bool UCC_HitReact::SetAvatarAILogicPaused(AActor* Avatar, bool bPaused)
{
	// 只对 AI 控制的角色生效：取 Pawn 的控制器并 Cast 到 AAIController；
	// 玩家（PlayerController）Cast 失败 → 安全空转返回 false。
	const APawn* Pawn = Cast<APawn>(Avatar);
	AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (!IsValid(AIController))
	{
		return false;
	}
	// 未运行行为树（无 BrainComponent）→ 安全空转，避免空指针。
	UBrainComponent* Brain = AIController->GetBrainComponent();
	if (!IsValid(Brain))
	{
		return false;
	}

	// PauseLogic/ResumeLogic 保留行为树运行状态，恢复后从暂停处继续（比 Stop/Restart 更贴合“暂时停止”）。
	static const FString PauseReason(TEXT("HitReaction"));
	if (bPaused)
	{
		Brain->PauseLogic(PauseReason);
		// PauseLogic 只暂停行为树 tick，不会停止已经发起的移动（PathFollowingComponent 独立于 BrainComponent）。
		// 必须显式 StopMovement 中止当前 MoveTo，否则受击时残留的追击移动仍会把敌人移向目标（尤其空中受击乱窜）。
		AIController->StopMovement();
	}
	else
	{
		Brain->ResumeLogic(PauseReason);
	}
	return true;
}

void UCC_HitReact::CacheHitDirectionVectors(const AActor* Instigator)
{
	AvatarForward = GetAvatarActorFromActorInfo()->GetActorForwardVector();

	const FVector avatarLocation = GetAvatarActorFromActorInfo()->GetActorLocation();
	const FVector InstigatorLocation = Instigator->GetActorLocation();

	ToInstigator = (InstigatorLocation-avatarLocation);
	ToInstigator.Normalize();
	
}

void UCC_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 必须在 Super 分发受击蓝图之前拦截，避免尸体重新创建动画任务。
	const ACC_BaseCharacter* HitCharacter = ActorInfo
		? Cast<ACC_BaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAbilitySystemComponent* HitASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(HitCharacter) || !HitCharacter->IsAlive() || HitCharacter->IsActorBeingDestroyed()
		|| !IsValid(HitASC) || HitASC->HasMatchingGameplayTag(CCTags::Status::Death))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	//缓存方向
	// CacheHitDirectionVectors(TriggerEventData->Instigator);
	// EHitDirection dir = UCC_BlueprintLibrary::GetHitDirection(AvatarForward,ToInstigator);
	// const FString dirS = UCC_BlueprintLibrary::GetHitDirectionName(dir);
	// const FName dirName(dirS);
	//
	//
	// // 播放蒙太奇
	// MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
	// 	this,
	// 	TEXT("PlayHitMontage"),
	// 	OwnedAction->Montage,
	// 	1.0f,
	// 	dirName,
	// 	true);
	//
	// MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	// MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
	// MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	// MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	// MontageTask->ReadyForActivation();

	BaseCharacter->HandleHit();
	
	// 方式二：屏幕打印，调试时更直观
	if (GEngine && ShowDebug)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("Ability Activated: %s"), *GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("Ability Activated: %s"), *GetName()));
	}
	
}

void UCC_HitReact::FinishOwnedAction(bool bWasCancelled)
{
	Super::FinishOwnedAction(bWasCancelled);

	BaseCharacter->HandleRefreshHit();
}



