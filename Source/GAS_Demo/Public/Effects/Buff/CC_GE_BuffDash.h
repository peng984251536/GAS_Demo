#pragma once
#include "GameplayEffect.h"
#include "CC_GE_BuffDash.generated.h"

/** 冲刺 Buff 的原生配置父类；没有运行时逻辑，能力由原生 Grant Abilities 组件授予和撤回。 */
UCLASS()

class GAS_DEMO_API UCC_GE_BuffDash : public UGameplayEffect
{
    GENERATED_BODY()
  public:
    UCC_GE_BuffDash();
};

/** 分裂箭的配置父类；具体发射规则仍由独立 Behavior 修改通用上下文。 */
UCLASS()

class GAS_DEMO_API UCC_GE_BuffSplitArrow : public UGameplayEffect
{
    GENERATED_BODY()
  public:
    UCC_GE_BuffSplitArrow();
};
