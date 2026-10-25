#pragma once
#include "GameplayEffectUIData.h"
#include "CC_BuffUIData.generated.h"

/**
 * 仅提供 Buff 栏的显示配置，客户端从 GE 类默认对象读取，不参与伤害和规则判定。
 */
UCLASS(DisplayName = "CC Buff UI Data")

class GAS_DEMO_API UCC_BuffUIData : public UGameplayEffectUIData
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Display")
	FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Display", meta = (MultiLine = true))
    FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Display")
	TObjectPtr<UTexture2D> Icon;
	
    /**
     * 只影响是否展示在 Buff 栏，不影响 GE 的实际生效、持续时间或规则执行。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Display") bool bHiddenInUI = false;
};
