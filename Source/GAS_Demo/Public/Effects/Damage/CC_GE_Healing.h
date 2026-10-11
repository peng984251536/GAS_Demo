#pragma once

#include "GameplayEffect.h"
#include "CC_GE_Healing.generated.h"

/**
 * 内置 Instant 治疗 GE，将正 SetByCaller 治疗量输出到 IncomingHealing。
 * 技能通过 UCC_DamageStatics::ApplyHealing 创建并应用 Spec。
 * 实际恢复量、生命上限限制和治疗数字由属性集与反馈组件处理。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_Healing : public UGameplayEffect
{
    GENERATED_BODY()
public:
    /**
     * 配置正数 SetByCaller 治疗量到 IncomingHealing 的中转 Modifier。
     */
    UCC_GE_Healing();
};
