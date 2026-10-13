#include "Data/Buff/CC_BuffDefinition.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "Effects/Buff/CC_GE_Buff.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/DataValidation.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
void UCC_BuffDefinition::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    // Loaded effect assets follow configuration edits; do not mutate templates used by a live game.
    if (GEngine)
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->HasBegunPlay()
                && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)) return;
    for (TObjectIterator<UCC_GE_Buff> It; It; ++It)
    {
        if (It->HasAnyFlags(RF_ClassDefaultObject) && !It->HasAnyFlags(RF_NeedPostLoad)
            && It->BuffConfiguration.Get() == GetClass())
        {
            It->OnGameplayEffectChanged();
            It->MarkPackageDirty();
        }
    }
}

EDataValidationResult UCC_BuffDefinition::IsDataValid(FDataValidationContext& Context) const
{
    bool bInvalid = false;
    if (SummonCount < 1 || SummonCount > 32 || !FMath::IsFinite(SummonRadius) || SummonRadius < 0.f)
    { Context.AddError(FText::FromString(TEXT("Invalid summon count or radius."))); bInvalid = true; }
    if (SummonClass && SummonClass->HasAnyClassFlags(CLASS_Abstract))
    { Context.AddError(FText::FromString(TEXT("Summon class must be concrete."))); bInvalid = true; }
    for (const auto& Behavior : Behaviors) if (!IsValid(Behavior))
    { Context.AddError(FText::FromString(TEXT("Empty special behavior entry."))); bInvalid = true; }
    const auto NativeResult = Super::IsDataValid(Context);
    return bInvalid || NativeResult == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
