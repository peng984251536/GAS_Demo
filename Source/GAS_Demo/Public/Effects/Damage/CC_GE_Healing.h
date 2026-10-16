#pragma once

#include "GameplayEffect.h"
#include "CC_GE_Healing.generated.h"

/**
 * 项目唯一的治疗 GE（Instant），由 UCC_DamageStatics::ApplyHealing 创建 Spec。
 * SetByCaller 治疗量 → Healing 中间属性；AttributeSet 负责限制到最大生命。
 * GameplayCue：GameplayCue.Combat.HealText，数值取 Healing 中间属性。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_Healing : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UCC_GE_Healing();
};
