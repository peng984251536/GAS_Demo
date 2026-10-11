#include "Effects/Damage/CC_GE_LyraStyleDamage.h"
#include "Damage/CC_DamageExecution.h"

/**
 * 配置 Instant 生命周期和伤害 Execution，不额外添加 Health 扣减 Modifier。
 */
UCC_GE_LyraStyleDamage::UCC_GE_LyraStyleDamage()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass = UCC_DamageExecution::StaticClass();
    Executions.Add(Execution);
}
