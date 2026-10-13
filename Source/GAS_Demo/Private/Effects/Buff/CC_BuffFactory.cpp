#include "Effects/Buff/CC_BuffFactory.h"
#include "Effects/Buff/CC_GE_Buff.h"
#include "Data/Buff/CC_BuffDefinition.h"
#include "AbilitySystemComponent.h"

FGameplayEffectSpecHandle UCC_BuffFactory::MakeBuffSpec(UAbilitySystemComponent* Source,
    TSubclassOf<UCC_GE_Buff> BuffEffect, float Level)
{
    if (!IsValid(Source) || !BuffEffect || BuffEffect->HasAnyClassFlags(CLASS_Abstract)
        || !FMath::IsFinite(Level) || Level <= 0.f) return {};
    const auto* Effect = BuffEffect.GetDefaultObject();
    if (!Effect || !Effect->GetBuffConfiguration()) return {};
    auto Context = Source->MakeEffectContext();
    Context.AddSourceObject(Effect->GetBuffConfiguration());
    // Effect CDO initialization happens in the native load/compile hooks, never on each pickup.
    return Source->MakeOutgoingSpec(BuffEffect, Level, Context);
}
