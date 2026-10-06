#include "Data/CC_CharacterConfig.h"

#include "Abilities/GameplayAbility.h"
#include "Character/Combat/CC_ArrowProjectile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UCC_CharacterConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < StartupAbilities.Num(); ++Index)
	{
		TSubclassOf<UCC_GameplayAbilityBase> Ability = StartupAbilities[Index];
		if (!IsValid(Ability) || Ability->HasAnyClassFlags(CLASS_Abstract))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("CCCharacterConfig", "InvalidAbility", "StartupAbilities[{0}] must specify a non-abstract GameplayAbility class."),
				FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		// if (Entry.AbilityLevel < 1)
		// {
		// 	Context.AddError(FText::Format(
		// 		NSLOCTEXT("CCCharacterConfig", "InvalidLevel", "StartupAbilities[{0}].AbilityLevel must be at least 1."),
		// 		FText::AsNumber(Index)));
		// 	Result = EDataValidationResult::Invalid;
		// }
	}
	// 弓兵必须有非零的后撤阈值。否则「目标过近」判断永远不成立，
	// 即使已经授予 KeepDistance 能力，运行时也不会看到任何后撤行为。
	if (ArrowProjectileClass && (MinRange <= 0.0f || MaxRange <= MinRange))
	{
		Context.AddError(NSLOCTEXT("CCCharacterConfig", "InvalidBowRange",
			"Ranged character requires 0 < MinRange < MaxRange."));
		Result = EDataValidationResult::Invalid;
	}
	// if (ArrowProjectileClass && (ArrowSpeed <= 0.0f || ArrowReleaseTime < 0.0f))
	// {
	// 	Context.AddError(NSLOCTEXT("CCCharacterConfig", "InvalidArrowTiming",
	// 		"ArrowSpeed must be positive and ArrowReleaseTime cannot be negative."));
	// 	Result = EDataValidationResult::Invalid;
	// }

	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif
