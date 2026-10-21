#include "Game/Survival/CC_SurvivalTags.h"

namespace CCSurvivalTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatKills, "Survival.Stat.Kills", "生存统计：击杀数");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatDeaths, "Survival.Stat.Deaths", "生存统计：阵亡次数");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatRevives, "Survival.Stat.Revives", "生存统计：复活次数");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatDamageDealt, "Survival.Stat.DamageDealt", "生存统计：造成伤害（取整累计）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatDamageTaken, "Survival.Stat.DamageTaken", "生存统计：承受伤害（取整累计）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatBuffsChosen, "Survival.Stat.BuffsChosen", "生存统计：选择的 Buff 数");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StatEnemiesSpawned, "Survival.Stat.EnemiesSpawned", "生存统计：刷出的怪物数");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgMatchStarted, "Survival.Msg.MatchStarted", "提示：对局开始");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgWaveStarted, "Survival.Msg.WaveStarted", "提示：新一波开始，Magnitude = 波次");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgLevelUp, "Survival.Msg.LevelUp", "提示：全队升级，Magnitude = 新等级");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgPlayerDown, "Survival.Msg.PlayerDown", "提示：玩家阵亡，Target = 阵亡者");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgPlayerRevived, "Survival.Msg.PlayerRevived", "提示：玩家复活，Target = 复活者");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MsgMatchEnded, "Survival.Msg.MatchEnded", "提示：对局结束");
}
