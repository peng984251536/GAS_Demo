#include "AI/BehaviorTree/BTTask_CC_ActivateAbility.h"

#include "AIController.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/ActionComponent.h"
#include "GAS_Demo.h"

UBTTask_CC_ActivateAbility::UBTTask_CC_ActivateAbility()
{
	NodeName = TEXT("Activate Ability");
	bNotifyTick = true;
	// 每个 AI 一份实例，ActiveASC 不会在多个 AI 之间共享。
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_CC_ActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	ACC_BaseCharacter* Character = AIController ? Cast<ACC_BaseCharacter>(AIController->GetPawn()) : nullptr;
	UCC_AbilitySystemComponent* ASC = Character
		? Cast<UCC_AbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
	UActionComponent* Actions = Character ? Character->GetUCombatActionComponent() : nullptr;
	if (!ASC || !Actions || !AbilityTag.IsValid())
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][BTTask] Activate Ability: missing ASC / ActionComponent / tag. Pawn=%s Tag=%s"),
			*GetNameSafe(Character), *AbilityTag.ToString());
		return EBTNodeResult::Failed;
	}

	const bool bTriggered = Actions->TryGameplayAbilityByTag(AbilityTag, ASC, ASC, FGameplayAbilityTargetDataHandle());
	if (ASC->IsAbilityWithTagActive(AbilityTag))
	{
		ActiveASC = ASC;
		return EBTNodeResult::InProgress;
	}
	// 能力当场就结束了（例如已经到达）算成功；根本没有激活算失败，让 Sequence / Selector 能据此往下走。
	return bTriggered ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

void UBTTask_CC_ActivateAbility::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const UCC_AbilitySystemComponent* ASC = ActiveASC.Get();
	if (!ASC || !ASC->IsAbilityWithTagActive(AbilityTag))
	{
		ActiveASC.Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_CC_ActivateAbility::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 行为树离开本节点时一并取消能力，避免能力脱离行为树继续运行。
	if (UCC_AbilitySystemComponent* ASC = ActiveASC.Get())
	{
		const FGameplayTagContainer Tags(AbilityTag);
		ASC->CancelAbilities(&Tags);
	}
	ActiveASC.Reset();
	return EBTNodeResult::Aborted;
}

FString UBTTask_CC_ActivateAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("Ability: %s"), *AbilityTag.ToString());
}
