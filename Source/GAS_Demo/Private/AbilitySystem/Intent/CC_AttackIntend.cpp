// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Intent/CC_AttackIntend.h"
#include "Character/Combat/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"
#include "GAS_Demo.h"

bool UCC_AttackIntend::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UCC_AttackIntend::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}
	if (!IsValid(BaseCharacter) || !IsValid(CombatComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 行为树只决定“现在攻击”；有箭投射物配置的角色把这次攻击交给射箭能力，
	// 否则仍走原来的近战 AttackAction。两种能力收到的仍是同一份动作数据。
	const UCC_CharacterConfig* Config = BaseCharacter->GetCharacterConfig();
	const bool bUseBowShoot = IsValid(Config) && Config->ArrowProjectileClass;
	const FGameplayTag ExecutionTag = bUseBowShoot
		? CCTags::CCAbilityTrigger::BowShoot
		: CCTags::CCAbilityTrigger::AttackAction;
	const bool bTriggered = CombatComponent->TryAttack(ExecutionTag);
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Intent] Attack routed: Enemy=%s Tag=%s Config=%s ArrowClass=%s Triggered=%d"),
		*GetNameSafe(BaseCharacter), *ExecutionTag.ToString(), *GetNameSafe(Config),
		IsValid(Config) ? *GetNameSafe(Config->ArrowProjectileClass.Get()) : TEXT("None"), bTriggered);
	
}

