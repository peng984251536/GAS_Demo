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

bool UCC_CombatRangeStatics::IsInAttackRange(const ACC_BaseCharacter* SelfCharacter, const AActor* Target)
{
	if (!IsValid(SelfCharacter) || !IsValid(Target))
	{
		return false;
	}
	if (!IsRangedCharacter(SelfCharacter))
	{
		return IsInMeleeReach(SelfCharacter, Target);
	}
	// 弓兵只看最大射程。太近（< MinRange）由行为树里排在射击前面的后撤分支处理；
	// 后撤失败（退无可退）时落到射击分支，贴脸也照样射，不会因为三个分支都不成立而原地发呆。
	const UCC_CharacterConfig* Config = SelfCharacter->GetCharacterConfig();
	const float Distance = FVector::Dist2D(SelfCharacter->GetActorLocation(), Target->GetActorLocation());
	return Config && Distance <= Config->MaxRange;
}

bool UCC_CombatRangeStatics::IsRangedCharacter(const ACC_BaseCharacter* Character)
{
	const UCC_CharacterConfig* Config = IsValid(Character) ? Character->GetCharacterConfig() : nullptr;
	return Config && Config->ArrowProjectileClass != nullptr;
}
