#include "AI/CC_CombatRangeStatics.h"

#include "Actors/Character/CC_BaseCharacter.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"

namespace
{
	constexpr float DefaultMeleeReach = 80.f;
}

float UCC_CombatRangeStatics::GetEdgeDistance2D(const AActor* A, const AActor* B)
{
	if (!IsValid(A) || !IsValid(B))
	{
		return TNumericLimits<float>::Max();
	}
	// ACharacter 的简单碰撞半径就是胶囊体半径，与 PathFollowing 的到达判定使用同一套数据。
	const float CenterDistance = FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation());
	return FMath::Max(0.f, CenterDistance - A->GetSimpleCollisionRadius() - B->GetSimpleCollisionRadius());
}

float UCC_CombatRangeStatics::GetMeleeReach(const ACC_BaseCharacter* Character)
{
	const UCC_CharacterConfig* Config = IsValid(Character) ? Character->GetCharacterConfig() : nullptr;
	return Config ? FMath::Max(0.f, Config->MeleeReach) : DefaultMeleeReach;
}

bool UCC_CombatRangeStatics::IsInMeleeReach(const ACC_BaseCharacter* SelfActor, const AActor* TargetActor)
{
	return IsValid(SelfActor) &&
		IsValid(TargetActor)&& GetEdgeDistance2D(SelfActor, TargetActor) <= GetMeleeReach(SelfActor);
}

bool UCC_CombatRangeStatics::IsRangedCharacter(const ACC_BaseCharacter* Character)
{
	const UCC_CharacterConfig* Config = IsValid(Character) ? Character->GetCharacterConfig() : nullptr;
	return Config && Config->ArrowProjectileClass != nullptr;
}
