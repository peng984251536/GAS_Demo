#include "Data/Buff/CC_BuffTags.h"

namespace CCBuffTags
{
UE_DEFINE_GAMEPLAY_TAG(ProjectileFire, "Hook.Projectile.Fire");
UE_DEFINE_GAMEPLAY_TAG(DashAbility, "Ability.Movement.Dash");
UE_DEFINE_GAMEPLAY_TAG(ProjectileDamage, "Damage.Source.Projectile");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(NotShared, "Buff.NotShared", "不同步给召唤物的 Buff");
}
