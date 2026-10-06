#pragma once

#include "CoreMinimal.h"
#include "Damage/GASDamageExecutionBase.h"
#include "GameplayEffect.h"
#include "CC_LyraStyleDamage.generated.h"

/**
 * 当前 GAS_Demo 的伤害 Execution 适配类。
 *
 * 只负责把项目属性绑定到通用 GASDamageExecutionBase：
 *   攻击方 UCC_AttributeSet.AttackPower -> 纯计算输入 AttackPower；
 *   受击方 UCC_AttributeSet.Health      -> 扣血上限及扣血输出目标。
 * 当前项目没有 Defense 属性，因此不绑定防御，按 0 处理。
 *
 * 只有适配实现需要包含 UCC_AttributeSet.h。换角色/属性集时可以另建适配类，
 * 不必让纯计算器或通用 GAS 层依赖新的角色、武器和属性类型。
 * 需要项目专属团队/免伤等规则时，可以在此类或其原生子类中重写 PrepareDamageInput。
 */
UCLASS()
class GAS_DEMO_API UCC_LyraStyleDamageExecution : public UGASDamageExecutionBase
{
    GENERATED_BODY()

public:
    /** 构造时完成固定属性绑定；本次攻击数值不会存储在 Execution 对象上。 */
    UCC_LyraStyleDamageExecution();
};

/**
 * 可直接创建 Spec 并应用的一次性伤害 GameplayEffect。
 *
 * 默认配置：Instant + 一条 UCC_LyraStyleDamageExecution。
 * 不配置额外 Health Modifier，避免同一次命中通过两条路径重复扣血。
 * 需要编辑器资产时，可创建此类的 GE 蓝图子类，继承这些默认配置。
 *
 * 使用链路：服务器确认命中 -> Source ASC 创建 Spec -> 填写 SetByCaller 参数
 *          -> 对 Target ASC 应用 Spec -> Execution 计算并输出 Health 扣减。
 *
 * 这是新增的 GE 类，不会自动替换原有 UCC_GE_Damage 或修改现有技能调用。
 * 持续攻击 Buff 仍通过单独的 Duration/Infinite GE 修改属性，不能用本 Instant GE 代替。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_LyraStyleDamage : public UGameplayEffect
{
    GENERATED_BODY()

public:
    /** 设置 Instant 生命周期并登记伤害 Execution，仅配置定义，不在构造函数中造成伤害。 */
    UCC_GE_LyraStyleDamage();
};
