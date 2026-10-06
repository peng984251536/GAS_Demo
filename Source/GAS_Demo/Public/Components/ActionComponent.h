// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Data\AbilityData\CombatActionData.h"
#include "Data\AbilityData\CombatActionSet.h"

#include "Components/ActorComponent.h"
#include "ActionComponent.generated.h"

/**
 * 这个组件保存“当前第几段、是否在输入窗口内、当前动作是谁”。把它添加到玩家或怪物角色上
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionComponent();

	
	/** 由输入或 AI 调用。首次输入从第 0 段开始，窗口内输入进入下一段。 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TryAttack(FGameplayTag GameplayTag);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TryDodge(FGameplayTag GameplayTag);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TryGameplayAbilityByTag(FGameplayTag GameplayTag,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		const FGameplayAbilityTargetDataHandle& TargetData
		= FGameplayAbilityTargetDataHandle());

	/** 动画通知状态开始时调用。 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OpenComboWindowForMontage(const UAnimSequenceBase* SourceAnimation);

	/** 动画通知状态结束时调用。 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CloseComboWindowForMontage(const UAnimSequenceBase* SourceAnimation);

	/** 由 GameplayAbility 在确认要播放本段时调用。返回本段的唯一代次。 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	int32 GetAttackIndex(UCombatActionData* Action);

	/** 只有仍属于本段的回调才可以结束动作，避免旧动画误清理新动作。 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndActionIfOwned(const UCombatActionData* ExpectedAction);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	UCombatActionData* GetCurrentAction() const { return CurrentAction; }
	UFUNCTION(BlueprintCallable, Category = "Combat")
	int32 GetCurrentGeneration() const { return CurrentGeneration; }
	UFUNCTION(BlueprintCallable, Category = "Combat")
	int32 GetNextComboIndex() const { return NextComboIndex; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	UCC_AbilitySystemComponent* ResolveASC() const;

	// UPROPERTY(Transient)
	// TObjectPtr<UCC_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCombatActionData> CurrentAction;

	int32 CurrentGeneration = 0;
	int32 NextComboIndex = 0;
	bool bCanCombo = false;
};
