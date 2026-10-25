#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CC_InAttackRange.generated.h"

/**
 * 行为树装饰器：受控角色现在能不能攻击黑板里的目标。
 *   - 近战：双方胶囊体边缘距离 ≤ 角色配置的 MeleeReach（与 CC_FollowTarget 的停止距离一致）；
 *   - 弓兵：水平中心距离在 MinRange～MaxRange 之间（与 UCC_BowShoot 的射程检查一致）。
 * 目标为空或无效时为 false。替代蓝图装饰器 BTD_Distance / BTD_Distance_Find，避免「停下的位置」与
 * 「能不能打」用两套标准。见 Docs/AI-Combat.zh-CN.md 第 7 节。
 *
 * 不支持 Observer Aborts：每次 Selector 重新选分支时检查即可。距离每帧都在变，打断会让攻击与追击来回切换。
 */
UCLASS(DisplayName = "CC In Attack Range")
class GAS_DEMO_API UBTDecorator_CC_InAttackRange : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CC_InAttackRange();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 目标所在的黑板键（Object，Actor）。默认 TargetActor。 */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FBlackboardKeySelector TargetKey;

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
