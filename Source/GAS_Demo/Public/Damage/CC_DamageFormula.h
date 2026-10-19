// 纯伤害公式：只做数值计算，不依赖 Unreal / GAS，便于独立单元测试（Tests/Standalone）。
#pragma once

#include <cmath>

namespace CCDamageFormula
{
	/** 一次命中的数值输入。除 OutgoingBonus（>= -1）外都要求有限且 >= 0。 */
	struct FInput
	{
		/** 已含 Buff 的攻击力。 */
		double AttackPower = 0.0;
		/** 攻击力系数。 */
		double SkillScale = 1.0;
		/** 额外固定伤害。 */
		double FlatDamage = 0.0;
		/** 技能侧额外倍率。 */
		double DamageMultiplier = 1.0;
		/** 增伤比例：0.5 表示 +50%，-1 表示伤害归零。 */
		double OutgoingBonus = 0.0;
		/** 暴击倍率；未暴击传 1。 */
		double CriticalMultiplier = 1.0;
		/** 固定防御，从扣防御前的伤害中减去。 */
		double Defense = 0.0;
		/** 承伤倍率：0.8 表示减伤 20%。 */
		double IncomingMultiplier = 1.0;
	};

	/**
	 * (攻击 × 系数 + 固定) × (1 + 增伤) × 额外倍率 × 暴击 − 防御 → 不低于 0 → × 承伤倍率。
	 * @param OutDamage 最终伤害（>= 0），不裁剪到目标生命值。
	 * @return 输入非法或计算溢出时返回 false，此时 OutDamage 为 0。
	 */
	inline bool Calculate(const FInput& In, double& OutDamage)
	{
		OutDamage = 0.0;
		const double NonNegative[] = {In.AttackPower, In.SkillScale, In.FlatDamage, In.DamageMultiplier,
			In.CriticalMultiplier, In.Defense, In.IncomingMultiplier};
		for (const double Value : NonNegative)
		{
			if (!std::isfinite(Value) || Value < 0.0) return false;
		}
		if (!std::isfinite(In.OutgoingBonus) || In.OutgoingBonus < -1.0) return false;

		const double BeforeDefense = (In.AttackPower * In.SkillScale + In.FlatDamage) * (1.0 + In.OutgoingBonus)
			* In.DamageMultiplier * In.CriticalMultiplier;
		// 中间值溢出为无穷时，乘 0 会得到 NaN，这里一并拦截。
		if (!std::isfinite(BeforeDefense)) return false;
		const double AfterDefense = BeforeDefense > In.Defense ? BeforeDefense - In.Defense : 0.0;
		const double Final = AfterDefense * In.IncomingMultiplier;
		if (!std::isfinite(Final)) return false;
		OutDamage = Final;
		return true;
	}
}
