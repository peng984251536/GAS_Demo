#include "GameplayTags/CC_Tags.h"

namespace CCTags
{
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Primary);

	namespace CCAbilities
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Attack,
			"CCTags.CCAbilities.Attack",
			"角色处于攻击状态"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Primary,
			"CCTags.CCAbilities.Primary",
			"进攻能力状态"
		);


		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			ActivateOnGiven,
			"CCTags.CCAbilities.ActivateOnGiven",
			"自动激活的能力状态"
		);

		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Move,
			"CCTags.CCAbilities.Move",
			"移动。"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Projectile,
			"CCTags.SetByCaller.Projectile",
			"Tag for Set by Caller Magnitude for Projectiles"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			MeleeAttack,
			"CCTags.SetByCaller.MeleeAttack",
			"Tag for Set by Caller MeleeAttack"
		);
	}

	namespace CCAbilityTrigger
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			FindPlayerTarget,
			"CCTags.CCAbilityTrigger.FindPlayerTarget",
			"触发一次玩家目标查找"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			FindRangedTarget,
			"CCTags.CCAbilityTrigger.FindRangedTarget",
			"触发一次远程目标查找"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			AttackAction,
			"CCTags.CCAbilityTrigger.AttackAction",
			"触发攻击动作能力"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(KeepDistance, "CCTags.CCAbilityTrigger.KeepDistance", "弓兵与目标拉开距离");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(BowShoot, "CCTags.CCAbilityTrigger.BowShoot", "弓兵发射箭矢");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(FollowTarget, "CCTags.CCAbilityTrigger.FollowTarget", "触发跟随目标能力");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			DodgeAction,
			"CCTags.CCAbilityTrigger.DodgeAction",
			"触发闪避动作能力"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			BeHit,
			"CCTags.CCAbilityTrigger.BeHit",
			"触发受击能力"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Death,
			"CCTags.CCAbilityTrigger.Death",
			"触发死亡能力"
		);
	}

	namespace Status
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Invincible,
			"CCTags.Status.Invincible",
			"角色处于无敌状态"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			SuperArmor,
			"CCTags.Status.SuperArmor",
			"角色处于霸体状态"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			Death,
			"CCTags.Status.Death",
			"角色处于死亡状态"
		);
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(
			BeHit,
			"CCTags.Status.BeHit",
			"角色处于受击状态"
		);
	}

	namespace UILayer
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Game, "UI.Layer.Game", "HUD 层：不响应返回，不抢焦点");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameMenu, "UI.Layer.GameMenu", "玩法内菜单层：背包、记分板");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Menu, "UI.Layer.Menu", "菜单层：主菜单、暂停、设置");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modal, "UI.Layer.Modal", "弹窗层：确认框");
	}
}
