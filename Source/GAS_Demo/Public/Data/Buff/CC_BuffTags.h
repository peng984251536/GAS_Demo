#pragma once
#include "NativeGameplayTags.h"

namespace CCBuffTags
{
GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ProjectileFire);
GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DashAbility);
GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ProjectileDamage);
/** 不同步给召唤物的 Buff（GE 资产标签），见 UCC_SummonBuffShareComponent。 */
GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NotShared);
}
