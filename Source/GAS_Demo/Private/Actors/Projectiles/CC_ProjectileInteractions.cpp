#include "Actors/Projectiles/CC_ProjectileInteractions.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/CC_Tags.h"
#include "CollisionShape.h"
#include "GameFramework/Pawn.h"

namespace CCProjectileTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(CanDeflect, "Ability.Deflect.Projectile", "攻击有效帧可以反弹敌方投射物");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(DeflectedDamage, "Damage.Source.Deflected", "被反弹投射物造成的伤害");
}

int32 CCProjectileInteractions::DeflectInShape(AActor* Attacker, const FVector& Location,
    const FQuat& Rotation, const FCollisionShape& Shape)
{
    auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker);
    if (!IsValid(Attacker) || !Attacker->HasAuthority() || !Attacker->GetWorld() || !ASC ||
        !ASC->HasMatchingGameplayTag(CCProjectileTags::CanDeflect) ||
        !ASC->HasMatchingGameplayTag(CCTags::CCAbilities::Attack)) return 0;

    // 单独查询 WorldDynamic，不把箭混入角色伤害目标列表。
    FCollisionObjectQueryParams Objects(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MeleeProjectileInteraction), false, Attacker);
    TArray<FOverlapResult> Overlaps;
    Attacker->GetWorld()->OverlapMultiByObjectType(Overlaps, Location, Rotation, Objects, Shape, Query);
    int32 Count = 0;
    for (const auto& Hit : Overlaps)
        if (auto* Arrow = Cast<ACC_ArrowProjectile>(Hit.GetActor()))
            if (Arrow->Deflect(Cast<APawn>(Attacker))) ++Count;
    return Count;
}
