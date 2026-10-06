#pragma once

#include <cmath>

/**
 * 纯伤害计算层：只接收数值并返回结果，不依赖 Unreal 或 GAS。
 *
 * 调用方负责：读取已包含 Buff 的属性、判断命中/暴击/团队关系、提供倍率。
 * 计算器负责：按固定顺序计算伤害，检查输入和算术溢出。
 * 输出接入层负责：扣血、护盾、死亡通知、飘字等游戏状态与表现。
 *
 * 相同输入总是得到相同结果；这里没有随机数、Actor 查询或属性写入。
 * 使用独立命名空间，避免与原项目的 CombatDamage::FCalculator/FModifier 混淆。
 */
namespace StandaloneDamage
{
    /** 计算状态。零伤害不代表失败，必须通过此状态区分。 */
    enum class EError
    {
        // 成功，包括被规则禁止、高防御或零倍率导致的零伤害。
        None,
        // 至少一个输入为负数、NaN 或无穷大。
        InvalidInput,
        // 输入各自有限，但乘加后的中间值或最终值超出 double 可表示范围。
        Overflow
    };

    /**
     * 一次命中的完整数值输入。
     *
     * 所有 double 字段都要求有限且 >= 0；不通过负数表示治疗。
     * 倍率 1 表示不改变伤害，0 表示该倍率对应的伤害归零。
     * 攻防属性应由外层完成 Buff 聚合，不要再把同一 Buff 重复应用到这些数值上。
     */
    struct FInput
    {
        /**
         * 已包含 Buff 的攻击力数值，不是倍率。
         * 例如基础攻击 100，Buff 提高到 150，这里直接传 150。
         */
        double AttackPower = 0.0;

        /**
         * 技能的攻击力系数：2 表示使用 200% 攻击力。
         * 只作用于 AttackPower，不作用于 FlatDamage。
         * 纯固定伤害技能可以传 0，再通过 FlatDamage 指定伤害。
         */
        double SkillScale = 1.0;

        /**
         * 技能额外固定伤害，在 AttackPower * SkillScale 之后相加。
         * 后续仍受距离、命中、防御和承伤倍率影响，并非真实伤害。
         */
        double FlatDamage = 0.0;

        /**
         * 已包含 Buff 的目标防御，直接从命中倍率处理后的伤害中扣除。
         * 例如防御前伤害 100、防御 30，防御后得到 70。
         * 这里不是百分比护甲公式；防御超过伤害时归零，不转为治疗。
         */
        double Defense = 0.0;

        /**
         * 距离系数，例如 0.5 表示距离衰减到 50%。
         * 调用方读取武器曲线后传入；计算器不会查询武器或测量距离。
         */
        double DistanceMultiplier = 1.0;

        /**
         * 本次命中的综合倍率，在扣防御之前相乘。
         * 例如暴击 x2、部位 x1.5，可由调用方合并后传入 3。
         * 不会自动判定暴击、识别骨骼或聚合普通属性 Buff。
         */
        double HitMultiplier = 1.0;

        /**
         * 扣防御后的承伤倍率：0.7 表示减伤 30%，1.5 表示易伤 50%。
         * 如果它来自多个 Buff，应由外层按约定的 GAS 属性聚合规则求值。
         */
        double IncomingMultiplier = 1.0;

        /**
         * 外部团队、无敌等规则给出的许可结果。
         * false 时返回成功的零伤害；但仍先校验所有数值，不隐藏非法配置。
         */
        bool bDamageAllowed = true;
    };

    /**
     * 计算结果与可用于战斗日志的中间值。
     *
     * Calculate 失败时只设置 Error，所有伤害字段恢复默认的 0。
     * 调用方应先检查 IsValid()，再使用数值，不能仅凭 FinalDamage==0 判断失败。
     */
    struct FResult
    {
        /** 默认成功；Calculate 遇到错误时返回相应错误码。 */
        EError Error = EError::None;

        /** 攻击力 * 技能系数 + 技能固定伤害，尚未处理命中环境。 */
        double RawDamage = 0.0;

        /** RawDamage * 距离系数 * 命中倍率，即扣防御之前的伤害。 */
        double DamageBeforeDefense = 0.0;

        /** max(0, DamageBeforeDefense - Defense)，尚未应用承伤倍率。 */
        double DamageAfterDefense = 0.0;

        /**
         * 理论最终伤害：DamageAfterDefense * IncomingMultiplier。
         * 不裁剪到目标生命值。例如目标剩 5 血，这里仍可返回 100 理论伤害。
         * 实际扣血量由接入层和 AttributeSet 决定，不能用本字段直接代表实际掉血。
         */
        double FinalDamage = 0.0;

        /** true 表示运算成功，不表示一定造成了大于零的伤害。 */
        bool IsValid() const { return Error == EError::None; }
    };

    /**
     * 计算一次命中的理论伤害，不修改输入或任何游戏对象。
     *
     * 固定顺序：
     *   攻击力 * 技能系数 + 固定伤害
     *   -> 乘距离系数 -> 乘命中倍率 -> 扣固定防御并归零 -> 乘承伤倍率。
     *
     * 示例：(100 * 2 + 20) * 0.5 * 2 = 220；扣 30 防御为 190；
     *       再乘 0.8 承伤倍率，最终为 152。
     *
     * @param Input 已准备好的一次命中参数，所有数值必须有限且非负。
     * @return 成功时包含中间值及最终伤害；失败时包含错误码且伤害值均为 0。
     */
    inline FResult Calculate(const FInput& Input)
    {
        // 1. 统一校验完整输入，即使本次禁止伤害，也能发现非法配置。
        const double Values[] = {
            Input.AttackPower, Input.SkillScale, Input.FlatDamage, Input.Defense,
            Input.DistanceMultiplier, Input.HitMultiplier, Input.IncomingMultiplier
        };
        for (const double Value : Values)
        {
            if (!std::isfinite(Value) || Value < 0.0)
            {
                return { EError::InvalidInput };
            }
        }

        FResult Result;
        // 2. 外部规则禁止伤害属于正常结果；不执行后续公式。
        if (!Input.bDamageAllowed)
        {
            return Result;
        }

        // 3. 先形成技能伤害，再应用距离衰减。固定伤害不受 SkillScale 放大。
        Result.RawDamage = Input.AttackPower * Input.SkillScale + Input.FlatDamage;
        Result.DamageBeforeDefense = Result.RawDamage * Input.DistanceMultiplier;
        // 每个阶段检查溢出，不能让后续的零倍率掩盖错误。
        if (!std::isfinite(Result.RawDamage) || !std::isfinite(Result.DamageBeforeDefense))
        {
            return { EError::Overflow };
        }
        // 4. 暴击/部位等倍率发生在扣防御之前，改变顺序会改变数值结果。
        Result.DamageBeforeDefense *= Input.HitMultiplier;
        if (!std::isfinite(Result.DamageBeforeDefense))
        {
            return { EError::Overflow };
        }

        // 5. 固定防御最多抵消本次伤害；不允许减出负数并意外变成治疗。
        const double AfterDefense = Result.DamageBeforeDefense - Input.Defense;
        Result.DamageAfterDefense = AfterDefense > 0.0 ? AfterDefense : 0.0;
        // 6. 最后应用承伤倍率。此处只产生数值，Health 的裁剪和扣减由外层负责。
        Result.FinalDamage = Result.DamageAfterDefense * Input.IncomingMultiplier;
        if (!std::isfinite(Result.FinalDamage))
        {
            return { EError::Overflow };
        }
        return Result;
    }
}
