#include "Utils/Team/CC_TeamStatics.h"

#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/CC_Tags.h"

namespace
{
/**
 * 把非角色 Actor（投射物等）解析到发起它的角色；角色本身原样返回。
 * 只沿 Instigator 查找一层，避免环形引用导致死循环。
 * @param Actor 待解析的 Actor。
 * @return 对应的角色；找不到时返回 nullptr。
 */
const ACC_BaseCharacter* ResolveCharacter(const AActor* Actor)
{
	if (const auto* Character = Cast<ACC_BaseCharacter>(Actor))
	{
		return Character;
	}
	return IsValid(Actor) ? Cast<ACC_BaseCharacter>(Actor->GetInstigator()) : nullptr;
}

/**
 * 角色自身的基础阵营：实例覆盖（召唤物等）优先，其次配置，最后角色类默认值。
 * @param Character 已解析的角色，可为空。
 * @return 基础阵营，没有时为空标签。
 */
FGameplayTag GetCharacterBaseTeam(const ACC_BaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return FGameplayTag();
	}
	const FGameplayTag Override = Character->GetTeamOverride();
	if (Override.IsValid())
	{
		return Override;
	}
	const UCC_CharacterConfig* Config = Character->GetCharacterConfig();
	if (Config && Config->Team.IsValid())
	{
		return Config->Team;
	}
	return Character->GetDefaultTeam();
}

/**
 * 查找授予 CCTags.Status.Charmed 的活动 GE 中最新的一个，返回它的施加者。
 * 被抑制的魅惑效果不生效。施加者从 GE Context 的 OriginalInstigator 读取，
 * 因此施加魅惑的 GE 不需要额外记录任何数据。
 * @param ASC 被魅惑角色的 ASC。
 * @return 魅惑者；没有生效中的魅惑效果时返回 nullptr。
 */
const AActor* FindCharmer(const UAbilitySystemComponent* ASC)
{
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CCTags::Status::Charmed.GetTag()));
	const AActor* Charmer = nullptr;
	float LatestStart = -FLT_MAX;
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (!Effect || Effect->bIsInhibited || Effect->StartWorldTime < LatestStart)
		{
			continue;
		}
		LatestStart = Effect->StartWorldTime;
		Charmer = Effect->Spec.GetContext().GetOriginalInstigator();
	}
	return Charmer;
}
}

bool UCC_TeamStatics::IsHostile(const AActor* A, const AActor* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}
	const FGameplayTag TeamA = GetEffectiveTeam(A);
	const FGameplayTag TeamB = GetEffectiveTeam(B);
	return TeamA.IsValid() && TeamB.IsValid() && TeamA != TeamB;
}

/**
 * 获取阵营信息
 * @param Actor 
 * @return 
 */
FGameplayTag UCC_TeamStatics::GetEffectiveTeam(const AActor* Actor)
{
	const ACC_BaseCharacter* Character = ResolveCharacter(Actor);
	if (!IsValid(Character))
	{
		return FGameplayTag();
	}
	
	// 被魅惑时跟随魅惑者的基础阵营。读基础阵营而不是有效阵营，避免互相魅惑时递归。
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(CCTags::Status::Charmed))
	{
		const ACC_BaseCharacter* Charmer = ResolveCharacter(FindCharmer(ASC));
		if (Charmer && Charmer != Character)
		{
			const FGameplayTag CharmerTeam = GetCharacterBaseTeam(Charmer);
			if (CharmerTeam.IsValid())
			{
				return CharmerTeam;
			}
		}
	}
	return GetCharacterBaseTeam(Character);
}

FGameplayTag UCC_TeamStatics::GetBaseTeam(const AActor* Actor)
{
	return GetCharacterBaseTeam(ResolveCharacter(Actor));
}
