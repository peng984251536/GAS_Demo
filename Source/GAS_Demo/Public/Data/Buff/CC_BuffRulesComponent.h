#pragma once
#include "GameplayEffectComponent.h"
#include "CC_BuffRulesComponent.generated.h"
class UCC_BuffBehavior;

/** Immutable GE configuration. Instances belong to the target's BuffComponent. */
UCLASS(DisplayName="CC Buff Rules")
class GAS_DEMO_API UCC_BuffRulesComponent : public UGameplayEffectComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Instanced, Category="Buff") TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;
    UPROPERTY(EditDefaultsOnly, Category="Buff") bool bRemoveOnDeath = true;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
