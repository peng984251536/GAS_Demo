#include "Effects/Buff/CC_GE_Buff.h"
#include "Data/Buff/CC_BuffDefinition.h"
#include "GameplayEffectComponent.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

void UCC_GE_Buff::PostInitProperties()
{
    Super::PostInitProperties();
    // Native subclasses may assign configuration in their constructor; Blueprint defaults arrive during load/compile.
    if (GetClass()->HasAnyClassFlags(CLASS_Native) && BuffConfiguration) OnGameplayEffectChanged();
}

const UCC_BuffDefinition* UCC_GE_Buff::GetBuffConfiguration() const
{
    return BuffConfiguration ? BuffConfiguration.GetDefaultObject() : nullptr;
}

void UCC_GE_Buff::InitializeFromConfiguration()
{
    if (bInitializingFromConfiguration || !BuffConfiguration) return;
    TGuardValue<bool> Guard(bInitializingFromConfiguration, true);
    auto* Configuration = BuffConfiguration.GetDefaultObject();
    if (!Configuration) return;
    Configuration->ConditionalPostLoad();
    // Copy UE's own editable properties, including all magnitude types, stacking and executions.
    // No parallel modifier/trigger structs, restricted attribute list or bespoke stacking policy.
    for (TFieldIterator<FProperty> It(UGameplayEffect::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        FProperty* Property = *It;
        if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
            && Property->GetFName() != TEXT("GEComponents"))
            Property->CopyCompleteValue_InContainer(this, Configuration);
    }
    GEComponents.Reset();
    for (const auto& Component : Configuration->GetConfiguredComponents())
    {
        if (!Component) continue;
        // GE components resolve their owner through Outer: sharing config component pointers would target the wrong GE.
        const FName Name = MakeUniqueObjectName(this, Component->GetClass(), Component->GetFName());
        GEComponents.Add(DuplicateObject<UGameplayEffectComponent>(Component, this, Name));
    }
}

void UCC_GE_Buff::OnGameplayEffectChanged()
{
    InitializeFromConfiguration();
    // Native implementation refreshes granted/asset/blocked tag caches and component callbacks.
    Super::OnGameplayEffectChanged();
}

#if WITH_EDITOR
void UCC_GE_Buff::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    if (!HasAnyFlags(RF_NeedPostLoad)) OnGameplayEffectChanged();
    Super::PostEditChangeProperty(Event);
}

EDataValidationResult UCC_GE_Buff::IsDataValid(FDataValidationContext& Context) const
{
    bool bInvalid = false;
    if (!BuffConfiguration && GetClass() != StaticClass())
    {
        // A newly created Blueprint has no configuration until its defaults are edited. Application rejects it meanwhile.
        Context.AddWarning(FText::FromString(TEXT("Select a CC_BuffDefinition configuration Blueprint before applying this effect.")));
    }
    else if (BuffConfiguration && BuffConfiguration->HasAnyClassFlags(CLASS_Abstract))
    { Context.AddError(FText::FromString(TEXT("Buff configuration cannot be abstract."))); bInvalid = true; }
    const auto NativeResult = Super::IsDataValid(Context);
    return bInvalid || NativeResult == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
