// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "CC_AttributeChangeTask.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnAttributeChanged,
	FGameplayAttribute, Attribute,
	float, NewValue,
	float, OldValue);

/**
 * BlueprintType 表示蓝图的一种数据类型
 * ExposedAsyncProxy 作为变量保存
 */
UCLASS(BlueprintType,meta = (ExposedAsyncProxy = AsyncTask))
class GAS_DEMO_API UCC_AttributeChangeTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// 属性变化委托
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChanged OnAttributeChanged;

	// 一个静态的类？类似于单例模式？
	UFUNCTION(BlueprintCallable,meta = (BlueprintInternalUseOnly = "true"))
	static UCC_AttributeChangeTask* ListenForAttributeChange(
		UAbilitySystemComponent* AbilitySystemComponent,
			FGameplayAttribute Attribute);

	UFUNCTION(BlueprintCallable)
	void EndTask();

private:
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	FGameplayAttribute AttributeToListenFor;

	void AttributeChanged(const FOnAttributeChangeData& Data) const;
	
};
