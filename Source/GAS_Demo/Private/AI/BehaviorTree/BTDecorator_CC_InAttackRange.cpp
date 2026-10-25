#include "AI/BehaviorTree/BTDecorator_CC_InAttackRange.h"

#include "AIController.h"
#include "AI/CC_CombatRangeStatics.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

UBTDecorator_CC_InAttackRange::UBTDecorator_CC_InAttackRange()
{
	NodeName = TEXT("In Attack Range");
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CC_InAttackRange, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");

	// 只在选分支时检查，不观察、不打断（见类注释）。
	bAllowAbortNone = true;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;
}

void UBTDecorator_CC_InAttackRange::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

bool UBTDecorator_CC_InAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const ACC_BaseCharacter* Self = AIController ? Cast<ACC_BaseCharacter>(AIController->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Self || !Blackboard)
	{
		return false;
	}
	const AActor* Target = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(TargetKey.GetSelectedKeyID()));
	return UCC_CombatRangeStatics::IsInAttackRange(Self, Target);
}

FString UBTDecorator_CC_InAttackRange::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s 在攻击距离内（近战 MeleeReach / 弓兵射程）"),
		*Super::GetStaticDescription(), *TargetKey.SelectedKeyName.ToString());
}
