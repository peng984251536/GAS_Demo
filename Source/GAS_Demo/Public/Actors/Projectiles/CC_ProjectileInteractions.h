#pragma once
#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

struct FCollisionShape;

/** 投射物系统的通用资格和伤害来源标签，不依赖任何具体 Buff。 */
namespace CCProjectileTags
{
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CanDeflect);
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DeflectedDamage);
}

namespace CCProjectileInteractions
{
    /**
     * 在一次近战有效判定形状内反弹敌方箭；只在服务器执行。
     * 要求角色同时具有攻击中状态和反弹资格，不扩大原有角色受击检测范围。
     * 返回成功反弹的箭数；重复形状中的同一支箭由箭自身保证只反弹一次。
     */
    GAS_DEMO_API int32 DeflectInShape(AActor* Attacker, const FVector& Location,
        const FQuat& Rotation, const FCollisionShape& Shape);
}
