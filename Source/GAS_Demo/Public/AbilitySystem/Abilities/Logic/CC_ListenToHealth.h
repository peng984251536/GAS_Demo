#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbility.h"
#include "CC_ListenToHealth.generated.h"

struct FOnAttributeChangeData;

/** 持续监听 Health，在血量低于阈值且尚未死亡时尝试激活死亡能力。由调用方授予和激活。 */
UCLASS()
class GAS_DEMO_API UCC_ListenToHealth : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_ListenToHealth();

protected:
	/** 与原蓝图一致：Health < 0.1 时进入死亡判断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability|Health", meta=(ClampMin="0.0"))
	float DeathHealthThreshold = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability|Health")
	FGameplayTag DeadStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability|Health")
	FGameplayTag DeathAbilityTag;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void BeginDestroy() override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void StopListening();

	TWeakObjectPtr<UAbilitySystemComponent> ListeningASC;
	FDelegateHandle HealthChangedHandle;
	bool bActivatingDeathAbility = false;
};
