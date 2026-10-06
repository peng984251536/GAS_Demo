#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "NoMovementAnimNotifyState.generated.h"

/**
 * 覆盖该 Notify State 的时间内：
 * 1. 忽略玩家移动输入；
 * 2. 立即清空已有移动速度；
 * 3. State 结束时恢复移动输入。
 */
UCLASS(meta = (DisplayName = "No Movement"))
class GAS_DEMO_API UNoMovementAnimNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};