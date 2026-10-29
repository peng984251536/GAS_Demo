#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_CC_ActivateAbility.generated.h"

class UCC_AbilitySystemComponent;

/**
 * 行为树任务：按标签激活能力，等能力结束后完成。替代蓝图任务 BTT_ActiveAbility。
 *   - 激活：UActionComponent::TryGameplayAbilityByTag（向自己发送同名 GameplayEvent），与原蓝图一致；
 *   - 等待：能力仍激活（IsAbilityWithTagActive）时保持 InProgress，结束后返回 Succeeded；
 *     没有激活也没有在运行时返回 Failed；
 *   - 打断：行为树中止本节点时取消该能力。原蓝图不取消，能力会脱离行为树继续运行，
 *     移动类能力因此可能永远等不到结束（见 Docs/AI/AI-Combat.zh-CN.md 8.2）。
 */
UCLASS(DisplayName = "CC Activate Ability")
class GAS_DEMO_API UBTTask_CC_ActivateAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CC_ActivateAbility();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** 要激活的能力的触发标签（同时也是能力的资产标签）。 */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "CCTags.CCAbilityTrigger"))
	FGameplayTag AbilityTag;

private:
	/** 本次执行所用的 ASC（节点按 AI 实例化，见构造函数）。 */
	TWeakObjectPtr<UCC_AbilitySystemComponent> ActiveASC;
};
