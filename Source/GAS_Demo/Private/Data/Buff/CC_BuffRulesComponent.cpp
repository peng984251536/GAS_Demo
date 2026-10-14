#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"
#if WITH_EDITOR
EDataValidationResult UCC_BuffRulesComponent::IsDataValid(FDataValidationContext& Context) const
{
    bool bInvalid = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
    if (GetOwner()->DurationPolicy == EGameplayEffectDurationType::Instant && !Behaviors.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Buff behaviors require a duration or infinite GE.")));
        bInvalid = true;
    }
    for (const UCC_BuffBehavior* Behavior : Behaviors)
    {
        if (!Behavior || Behavior->GetClass()->HasAnyClassFlags(CLASS_Abstract))
        {
            Context.AddError(FText::FromString(TEXT("Buff behavior templates must be non-null and concrete.")));
            bInvalid = true;
        }
        else if (Behavior->IsDataValid(Context) == EDataValidationResult::Invalid) bInvalid = true;
    }
    return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
