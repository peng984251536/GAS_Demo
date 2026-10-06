#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_FindRangedTarget.generated.h"

class ACC_EnemyCharacter;

/**
 * 远程弓兵索敌：查找配置 Tag 指定的最近目标，
 * 超出最大射程时清除目标；目标过近时由独立的 KeepDistance 能力后退。
 */
UCLASS()
class GAS_DEMO_API UCC_FindRangedTarget : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_FindRangedTarget();

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/** 记录面向目标的攻击朝向并按给定结果结束能力。 */
	void FaceTargetAndEnd(ACC_EnemyCharacter& Enemy, const AActor& TargetActor, bool bWasCancelled);
};
