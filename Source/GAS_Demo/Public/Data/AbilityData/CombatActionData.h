// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "CombatActionData.generated.h"

class UAnimMontage;

/**
 * 动作数据资产：每个“动作结果”一份数据
 */
UCLASS()
class GAS_DEMO_API UCombatActionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 这一招实际播放的动画
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage;

	/** 连招序号：0 为首段，1 为第二段。-1 表示不参加普通连招。 */
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	// int32 ComboIndex = -1;
	
	// 用来决定由哪个通用 Ability 执行
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag MechanicTag;

	// 可选：伤害、冷却、命中效果、位移配置等也都放这里
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 10.f;
	
};
