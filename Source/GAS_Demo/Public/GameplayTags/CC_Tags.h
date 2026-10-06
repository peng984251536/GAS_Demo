#pragma once

#include "NativeGameplayTags.h"

namespace CCTags
{
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Primary);
	// 负责能力的枚举
	namespace CCAbilities
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Move);
		
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);

		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Projectile);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(MeleeAttack);
	}

	namespace CCAbilityTrigger
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(FindPlayerTarget);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(FindRangedTarget);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackAction);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(KeepDistance);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(BowShoot);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(FollowTarget);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(DodgeAction);

		UE_DECLARE_GAMEPLAY_TAG_EXTERN(BeHit);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
	}

	namespace Status
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invincible);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(SuperArmor);
		
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(BeHit);
	}

	// UI 根布局的四个页面层。名称与 DefaultGameplayTags.ini 中的 UI.Layer.* 保持一致，
	// C++ 一律引用这里的原生标签，避免字符串拼写错误在运行时静默返回空层。
	namespace UILayer
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Game);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameMenu);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Menu);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modal);
	}
}
