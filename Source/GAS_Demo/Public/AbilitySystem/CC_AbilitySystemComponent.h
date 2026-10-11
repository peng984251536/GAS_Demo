// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Utils/FBufferedAbilityInput.h"
#include "Damage/CC_DamageTypes.h"
#include "CC_AbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCC_AbilitySystemComponent();
    /**
     * 初始化 ASC 后在服务器 Owner 上创建或复用反馈组件，并刷新订阅。
     * @param Owner ASC 所属 Actor；玩家通常为 PlayerState，敌人通常为角色。
     * @param Avatar ASC 当前操作的 Avatar，通常是角色 Pawn。
     */
    virtual void InitAbilityActorInfo(AActor* Owner, AActor* Avatar) override;
    /** 项目级结算通知：供反馈、战斗日志等服务器逻辑订阅，不会自动复制。 */
    FCC_DamageResolvedDelegate OnDamageResolved;
    /**
     * 在权威端广播已确定的结算结果，不再次扣血或激活死亡。
     * @param Result 本次伤害/治疗的不可变结果副本，包含完整伤害、实际生命变化与归属。
     */
    void PublishDamageResult(const FCC_DamageResolved& Result);
    /** 自动反馈组件；玩家通常挂在 PlayerState（ASC Owner），敌人挂在角色 Owner。 */
    UPROPERTY(Transient) TObjectPtr<class UCC_CombatFeedbackComponent> CombatFeedback;

	void HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec );

	UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	void SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass,int32 Level);
	UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	void AddAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass,int32 Level);

	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool HandleComboInput(
	// const FGameplayTag& AttackAbilityTag);
	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool CancelAndActivate(
	// 	const FGameplayTag& CancelAbilityTag,
	// 	const FGameplayTag& NewAbilityTag);
	// UFUNCTION(BlueprintCallable,Category="Ability|Abilities")
	// bool HandleQueuedInput(
	// 	const FGameplayTag& WaitForAbilityTag,
	// 	const FGameplayTag& NewAbilityTag);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// bool ConsumeComboInput(
	// 	const FGameplayTag& AttackAbilityTag,
	// 	float MaxBufferTime = 0.25f);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void ClearBufferedInput();

protected:
	UPROPERTY(EditDefaultsOnly, Category="Ability|Input")
	float DefaultBufferTime = 0.25f;
	
	FBufferedAbilityInput BufferedInput;
	
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRep_ActivateAbilities() override;
	virtual FActiveGameplayEffectHandle ApplyEffectSpecToSelf(FGameplayTag& gameplayTag,UClass* gameplayEffect);

	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void HandleAbilityEnded(UGameplayAbility* EndedAbility);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// void CacheInput(
	// 	const FGameplayTag& AbilityTag,
	// 	const FGameplayTag& WaitForAbilityTag,
	// 	ECCAbilityLinkType LinkType);
	// UFUNCTION(BlueprintCallable, Category="Ability|Input")
	// bool IsBufferedInputFresh(float MaxBufferTime) const;
	UFUNCTION(BlueprintCallable, Category="Ability|Input")
	bool IsAbilityWithTagActive(const FGameplayTag& AbilityTag) const;
	

};
