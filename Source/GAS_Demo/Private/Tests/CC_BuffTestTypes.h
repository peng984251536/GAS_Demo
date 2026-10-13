#pragma once
#include "Data/Buff/CC_BuffDefinition.h"
#include "Effects/Buff/CC_GE_Buff.h"
#include "CC_BuffTestTypes.generated.h"

/**
 * 自动化测试使用的原生配置类型，覆盖 UE 的修饰器、组件和堆叠行为。
 * 这些类不由场景道具引用，不修改正式玩法配置。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestConfiguration : public UCC_BuffDefinition
{
    GENERATED_BODY()
public:
    UCC_BuffTestConfiguration();
};

/**
 * 自动化测试使用的周期配置，直接使用原生 Period 与 IncomingDamage 修饰器。
 * 用于确认周期结算由 GAS 驱动，组件只同步活动效果信息。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestPeriodicConfiguration : public UCC_BuffDefinition
{
    GENERATED_BODY()
public:
    UCC_BuffTestPeriodicConfiguration();
};

/**
 * 自动化测试使用的即时治疗配置，用于验证原生完成触发器。
 * Instant 效果仅执行一次，不进入角色的活动 buff 列表。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestInstantConfiguration : public UCC_BuffDefinition
{
    GENERATED_BODY()
public:
    UCC_BuffTestInstantConfiguration();
};

/**
 * 测试效果引用测试配置，通过正式的配置加载流程初始化原生 GE 属性。
 * 使用独立效果类，确保测试包含 GAS 按效果类识别堆叠的真实行为。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestEffect : public UCC_GE_Buff
{
    GENERATED_BODY()
public:
    UCC_BuffTestEffect();
};

/**
 * 第二个测试效果复用同一配置，用于验证不同效果类不会被错误合并。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestOtherEffect : public UCC_GE_Buff
{
    GENERATED_BODY()
public:
    UCC_BuffTestOtherEffect();
};

/**
 * 周期测试效果，生命周期和执行间隔均来自测试配置。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestPeriodicEffect : public UCC_GE_Buff
{
    GENERATED_BODY()
public:
    UCC_BuffTestPeriodicEffect();
};

/**
 * 即时治疗测试效果，可由原生 AdditionalEffects 组件在 buff 移除时触发。
 */
UCLASS(NotBlueprintable)
class UCC_BuffTestInstantEffect : public UCC_GE_Buff
{
    GENERATED_BODY()
public:
    UCC_BuffTestInstantEffect();
};
