#include "Damage/CC_LyraStyleDamage.h"

// 项目具体 AttributeSet 只在适配层引用，通用计算器和 GAS 桥接不需要知道这个类型。
#include "Attribute/CC_AttributeSet.h"

UCC_LyraStyleDamageExecution::UCC_LyraStyleDamageExecution()
{
    // Get...Attribute() 返回属性字段标识，不是当前数值；实际数值由 GAS 为每个 Spec 捕获。
    // 当前绑定把 AttackPower 当作数值攻击力，和旧 FCalculator 的同名“攻击倍率”语义不同。
    // 攻击默认采用快照：通常在 Source ASC 的 MakeOutgoingSpec 时捕获。
    //
    // 当前项目没有 Defense 属性，省略第三个参数表示固定防御为 0。
    // 日后添加 Defense，只需把对应 getter 作为第三个参数传入，无需改纯计算器。
    // 如需命中时读取攻击力，可显式把第四个参数 bSnapshotAttack 设为 false。
    ConfigureAttributes(
        UCC_AttributeSet::GetAttackPowerAttribute(),
        UCC_AttributeSet::GetHealthAttribute());
}

UCC_GE_LyraStyleDamage::UCC_GE_LyraStyleDamage()
{
    // Instant 表示应用时执行一次，输出永久作用于属性基础值的扣减，
    // 不像普通持续属性 Buff 那样在效果到期后撤销加成或“返还血量”。
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // 给 GE 的执行列表登记计算类。创建本 GE 的 Spec 后，GAS 会收集该类声明的属性捕获。
    // 应用到目标时由 GAS 调用 Execute_Implementation；业务代码只需要应用 Spec。
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass = UCC_LyraStyleDamageExecution::StaticClass();
    // 仅登记一条伤害 Execution；不要另外添加同一次命中的 Health 扣减 Modifier。
    Executions.Add(Execution);
}
