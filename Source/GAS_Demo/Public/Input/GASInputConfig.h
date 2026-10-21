// GASInputConfig.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"

#include "GASInputConfig.generated.h"

/***
 * 输入行为
 */
USTRUCT(BlueprintType)
struct FGASTaggedInputAction
{
	GENERATED_BODY()

	//TODO-
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag;
};


// 一个数据类
UCLASS(BlueprintType)
class UGASInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Native")
	TArray<FGASTaggedInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Ability")
	TArray<FGASTaggedInputAction> AbilityInputActions;

	void BuildCache();

	const UInputAction* FindNativeInputAction(const FGameplayTag& InputTag) const;
	const UInputAction* FindAbilityInputAction(const FGameplayTag& InputTag) const;

private:

	// 使用 Map 来管理不同类型的Action
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputAction>> NativeInputActionMap;
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputAction>> AbilityInputActionMap;
};



