#pragma once
#include "GameplayEffectUIData.h"
#include "CC_BuffUIData.generated.h"

UCLASS(DisplayName="CC Buff UI Data")
class GAS_DEMO_API UCC_BuffUIData : public UGameplayEffectUIData
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display", meta=(MultiLine=true)) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display") TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display") bool bHiddenInUI = false;
};
