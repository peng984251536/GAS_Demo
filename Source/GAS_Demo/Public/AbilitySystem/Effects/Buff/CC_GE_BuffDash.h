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

/**
 * 召唤 Buff 的原生配置父类：获得后立即在身边召唤一个随从，召唤后 Buff 自动移除。
 * 授予的 GA 默认是 UCC_SummonAlly；在蓝图子类的 Grant Abilities 组件里换成填好 SummonClass 的 GA 蓝图。
 */
UCLASS()

class GAS_DEMO_API UCC_GE_BuffSummon : public UGameplayEffect
{
    GENERATED_BODY()
  public:
    UCC_GE_BuffSummon();
};

/** 分裂箭的配置父类；具体发射规则仍由独立 Behavior 修改通用上下文。 */
UCLASS()

class GAS_DEMO_API UCC_GE_BuffSplitArrow : public UGameplayEffect
{
    GENERATED_BODY()
  public:
    UCC_GE_BuffSplitArrow();
};
