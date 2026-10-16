#include "Damage/CC_DamageTags.h"

namespace CCDamageTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SkillScale, "SetByCaller.Damage.SkillScale", "伤害：攻击力系数");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FlatDamage, "SetByCaller.Damage.FlatDamage", "伤害：额外固定伤害");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DamageMultiplier, "SetByCaller.Damage.Multiplier", "伤害：技能侧额外倍率");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HealingAmount, "SetByCaller.Healing.Amount", "治疗量");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity, "Status.DamageImmunity", "免疫伤害");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TypeRoot, "Damage.Type", "伤害类型根标签");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventDamageDealt, "Event.Combat.DamageDealt", "造成伤害后发给来源");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventDamageTaken, "Event.Combat.DamageTaken", "受到伤害后发给目标");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CueDamageText, "GameplayCue.Combat.DamageText", "伤害数字");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CueHealText, "GameplayCue.Combat.HealText", "治疗数字");
}
