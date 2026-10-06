#include "AbilitySystem/Abilities/Reaction/CC_Death.h"

#include "Character/CC_EnemyCharacter.h"
#include "GAS_Demo.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCC_Death::UCC_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// ASC 复制死亡蒙太奇，服务器销毁 Actor 后会同步到客户端。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
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
	if (!IsValid(BaseCharacter) || !BaseCharacter->HasAuthority())
	{
		FinishOwnedAction(true);
		return;
	}
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
	TWeakObjectPtr<ACC_EnemyCharacter> Enemy = Cast<ACC_EnemyCharacter>(GetAvatarActorFromActorInfo());
	Super::FinishOwnedAction(bWasCancelled);

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
