#include "AbilitySystem/Abilities/Mechanics/CC_SummonAlly.h"

#include "GAS_Demo.h"
#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "AI/CC_AIController.h"
#include "Components/CC_SummonBuffShareComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameplayTags/CC_Tags.h"
#include "TimerManager.h"
#include "Utils/Team/CC_TeamStatics.h"

UCC_SummonAlly::UCC_SummonAlly()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 生成 Actor 只能在服务器做；客户端不需要预测。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// 授予即激活（UCC_AbilitySystemComponent::HandleAutoActivatedAbility）。
	SetAssetTags(FGameplayTagContainer(CCTags::CCAbilities::ActivateOnGiven));
	ActivationBlockedTags.AddTag(CCTags::Status::Death);
}

void UCC_SummonAlly::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	ACC_EnemyCharacter* Summon = SpawnNearCaster();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FActiveGameplayEffectHandle GrantingBuff = ASC ? ASC->FindActiveGameplayEffectHandle(Handle)
		: FActiveGameplayEffectHandle();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

	// 召唤成功才消耗 Buff。本能力在 Buff 施加过程中被授予并激活，下一帧再移除，避免在 GE 施加流程中途移除它。
	if (Summon && ASC && GrantingBuff.IsValid())
	{
		ASC->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(ASC,
			[ASC, GrantingBuff]() { ASC->RemoveActiveGameplayEffect(GrantingBuff); }));
	}
}

ACC_EnemyCharacter* UCC_SummonAlly::SpawnNearCaster() const
{
	UWorld* World = IsValid(BaseCharacter) ? BaseCharacter->GetWorld() : nullptr;
	// 召唤物不能再召唤（防止召唤 Buff 漏标 Buff.NotShared 时无限连锁）。
	if (!World || !SummonClass || BaseCharacter->FindComponentByClass<UCC_SummonBuffShareComponent>())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] Skipped Caster=%s SummonClass=%s"),
			*GetNameSafe(BaseCharacter), *GetNameSafe(SummonClass.Get()));
		return nullptr;
	}

	// 让随从脚底与施放者脚底同高。
	const UCapsuleComponent* SummonCapsule = SummonClass->GetDefaultObject<ACC_EnemyCharacter>()->GetCapsuleComponent();
	const float HeightOffset = (SummonCapsule ? SummonCapsule->GetScaledCapsuleHalfHeight() : 88.f)
		- BaseCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
	const FVector Origin = BaseCharacter->GetActorLocation() + FVector(0.f, 0.f, HeightOffset);
	const float CasterYaw = BaseCharacter->GetActorRotation().Yaw;

	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Yaw = CasterYaw + 180.f + Index * 45.f;
		const FTransform SpawnTransform(FRotator(0.f, CasterYaw, 0.f), Origin + FRotator(0.f, Yaw, 0.f).Vector() * SpawnDistance);
		ACC_EnemyCharacter* Summon = World->SpawnActorDeferred<ACC_EnemyCharacter>(SummonClass, SpawnTransform,
			BaseCharacter, BaseCharacter, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
		if (!Summon)
		{
			continue;
		}
		// FinishSpawning 前写入：BeginPlay 时 AI 就按施放者阵营选目标。
		Summon->SetTeamOverride(UCC_TeamStatics::GetBaseTeam(BaseCharacter));
		Summon->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
		Summon->FinishSpawning(SpawnTransform);
		if (IsValid(Summon))
		{
			// 召唤者的 Buff 复制给随从，之后获得的 Buff 也同步一份（各自独立计时）。
			UCC_SummonBuffShareComponent* Share = NewObject<UCC_SummonBuffShareComponent>(Summon);
			Share->RegisterComponent();
			Share->StartSharing(BaseCharacter);
			// 随从默认跟随施放者：没有目标时回到身边，只打施放者附近的敌人，追远了放弃目标回来
			// （栓绳半径见 FCC_SquadOrder）。AutoPossessAI 已在 FinishSpawning 中完成附身。
			if (ACC_AIController* SummonController = Cast<ACC_AIController>(Summon->GetController()))
			{
				FCC_SquadOrder FollowOrder;
				FollowOrder.Order = ECC_SquadOrder::Follow;
				FollowOrder.Anchor = BaseCharacter.Get();
				SummonController->SetSquadOrder(FollowOrder);
			}
			else
			{
				UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] %s has no ACC_AIController; it will not follow %s"),
					*GetNameSafe(Summon), *GetNameSafe(BaseCharacter));
			}
			UE_LOG(LogGAS_Demo, Log, TEXT("[Summon] %s summoned %s"), *GetNameSafe(BaseCharacter), *GetNameSafe(Summon));
			return Summon;
		}
	}
	UE_LOG(LogGAS_Demo, Warning, TEXT("[Summon] No room around %s"), *GetNameSafe(BaseCharacter));
	return nullptr;
}
