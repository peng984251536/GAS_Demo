#include "AbilitySystem/Abilities/Reaction/CC_Death.h"

#include "Actors/Character/CC_EnemyCharacter.h"
#include "GAS_Demo.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCC_Death::UCC_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 死亡由服务器判定（生命归零），服务器先激活，再通知拥有者客户端激活并播放死亡动画。
	// 不能用 ServerOnly：ASC 的蒙太奇复制只发给非拥有者，死亡的玩家自己会看不到死亡动画。
	// 怪物没有拥有者客户端，行为与 ServerOnly 相同。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	bRetriggerInstancedAbility = false;
	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::Death));
	ActivationBlockedTags.AddTag(CCTags::Status::Death);
	ActivationOwnedTags.AddTag(CCTags::Status::Death);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CCTags::CCAbilityTrigger::Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

}

void UCC_Death::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UCC_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                const FGameplayAbilityActorInfo* ActorInfo,
                                const FGameplayAbilityActivationInfo ActivationInfo,
                                const FGameplayEventData* TriggerEventData)
{
	// 原生代码负责播放，不能再执行旧蓝图的激活事件，否则会创建第二个蒙太奇任务。
	if (!IsActive())
	{
		return;
	}
	if (!IsValid(BaseCharacter))
	{
		FinishOwnedAction(true);
		return;
	}
	// 死亡结算只在服务器生效（HandleDeath 内部有权限判断，客户端调用为空操作）。
	// 服务器与拥有者客户端都继续往下播放死亡蒙太奇。
	BaseCharacter->HandleDeath();
	if (!IsValid(OwnedAction) || !IsValid(OwnedAction->Montage))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("CC_Death: missing death montage."));
		FinishOwnedAction(true);
		return;
	}

	// 播放蒙太奇
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("PlayActionMontage"),
		OwnedAction->Montage,
		1.0f,
		NAME_None,
		true, 1.f, 0.f, true);
	if (!IsValid(MontageTask))
	{
		FinishOwnedAction(true);
		return;
	}
	
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UCC_Death::FinishOwnedAction(bool bWasCancelled)
{
	if (!IsActive() || bIsAbilityEnding)
	{
		return;
	}
	// 拥有者客户端只负责表现：本地结束，不把"结束"复制给服务器。
	// 否则客户端动画先播完时会提前结束服务器的死亡能力，绕过下面的 DeathFinished 上报，复活流程接不上。
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		if (IsValid(CombatComponent))
		{
			CombatComponent->EndActionIfOwned(OwnedAction);
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, bWasCancelled);
		return;
	}
	TWeakObjectPtr<ACC_BaseCharacter> Character = Cast<ACC_BaseCharacter>(GetAvatarActorFromActorInfo());
	TWeakObjectPtr<ACC_EnemyCharacter> Enemy = Cast<ACC_EnemyCharacter>(Character.Get());
	Super::FinishOwnedAction(bWasCancelled);

	// 死亡动作结束（DeathFinished）。复活会取消本能力，此时角色已活着，不再上报。
	if (!IsActive() && Character.IsValid() && Character->HasAuthority() && !Character->IsAlive())
	{
		Character->HandleDeathFinished();
	}

	// 先结束能力和动画任务，再销毁承载 ASC 的怪物，避免销毁期间重复结束能力。
	if (!IsActive() && Enemy.IsValid() && Enemy->HasAuthority())
	{
		// 当前仍可能在蒙太奇/伤害事件栈中，先退出交互，下一帧再销毁 ASC 所属 Actor。
		Enemy->SetActorEnableCollision(false);
		Enemy->SetActorHiddenInGame(true);
		Enemy->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Enemy]()
		{
			if (Enemy.IsValid())
			{
				Enemy->Destroy();
			}
		}));
	}
}
