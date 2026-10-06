// GASInputConfig.cpp

#include "Data/GASInputConfig.h"


/**
 * 初始化数据
 */
void UGASInputConfig::BuildCache()
{
	NativeInputActionMap.Reset();
	AbilityInputActionMap.Reset();

	for (const FGASTaggedInputAction& ActionConfig : NativeInputActions)
	{
		if (ActionConfig.InputAction && ActionConfig.InputTag.IsValid())
		{
			NativeInputActionMap.Add(ActionConfig.InputTag, ActionConfig.InputAction);
		}
	}

	for (const FGASTaggedInputAction& ActionConfig : AbilityInputActions)
	{
		if (ActionConfig.InputAction && ActionConfig.InputTag.IsValid())
		{
			AbilityInputActionMap.Add(ActionConfig.InputTag, ActionConfig.InputAction);
		}
	}
}

const UInputAction* UGASInputConfig::FindNativeInputAction(const FGameplayTag& InputTag) const
{
	const TObjectPtr<const UInputAction>* FoundAction = NativeInputActionMap.Find(InputTag);
	return FoundAction ? FoundAction->Get() : nullptr;
}

const UInputAction* UGASInputConfig::FindAbilityInputAction(const FGameplayTag& InputTag) const
{
	const TObjectPtr<const UInputAction>* FoundAction = AbilityInputActionMap.Find(InputTag);
	return FoundAction ? FoundAction->Get() : nullptr;
}