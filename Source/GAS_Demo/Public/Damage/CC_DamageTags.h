// 伤害链路使用的全部原生标签。只声明标签，不依赖任何伤害类。
#pragma once

#include "NativeGameplayTags.h"

namespace CCDamageTags
{
	// --- SetByCaller：技能传入的数值 ---
	/** 攻击力系数。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillScale);
	/** 额外固定伤害。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlatDamage);
	/** 技能侧已算好的额外倍率（距离衰减、弱点等）。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageMultiplier);
	/** 治疗量。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HealingAmount);

	// --- 状态 ---
	/** 目标带此标签时伤害 GE 不生效（由 GE 的 Tag Requirements 拦截）。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity);

	// --- 伤害类型根标签：具体类型写成 Damage.Type.Fire 等子标签 ---
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TypeRoot);

	// --- 结算后在服务器发送的 GameplayEvent ---
	/** 发给伤害来源：Instigator=来源，Target=受击者，EventMagnitude=实际扣血。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventDamageDealt);
	/** 发给受击者，内容与 DamageDealt 相同。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventDamageTaken);

	// --- 表现：配置在伤害/治疗 GE 上的 GameplayCue ---
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CueDamageText);
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CueHealText);
}
