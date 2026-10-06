// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AbilityData/CombatActionData.h"
#include "CombatActionSet.generated.h"

/**
 * 一个角色、武器或怪物类型所拥有的动作目录。
 * 不同怪物使用不同 ActionSet，即可拥有不同的动画和连招。
 */
UCLASS()
class GAS_DEMO_API UCombatActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TArray<TObjectPtr<UCombatActionData>> Actions;

	UCombatActionData* FindActionByComboIndex(int32 ComboIndex) const;
	int FindActionIndexByCombo(UCombatActionData* Data) const;
	int32 GetComboLength() const;
	
	
};
