#pragma once

#include "CoreMinimal.h"
#include "Components/CC_BuffSelectionTypes.h"
#include "CC_BuffSelectionViewTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * 一张三选一卡片的展示数据：选项下标加上从 GE 的 UCC_BuffUIData 读出的名称、描述和图标。
 * 只用于显示；选择时只回传 OptionIndex，服务器以自己保存的选项为准。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_BuffOfferCardData
{
    GENERATED_BODY()

    /** 在本轮选项中的下标，点击后原样交给 Choose。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    int32 OptionIndex = INDEX_NONE;

    /** 对应的 Buff GE 类，蓝图可据此读取更多自定义显示信息。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    TSubclassOf<UGameplayEffect> Effect;

    /** 施加时使用的 GE 等级。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    float Level = 1.f;

    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "Buff|Selection")
    TObjectPtr<UTexture2D> Icon;

    /** 从一个选项生成卡片数据；GE 缺少 UIData 时名称回退为类名。 */
    static FCC_BuffOfferCardData FromOption(const FCC_BuffOfferOption& Option, int32 Index);

    bool operator==(const FCC_BuffOfferCardData& Other) const
    {
        return OptionIndex == Other.OptionIndex && Effect == Other.Effect && Level == Other.Level;
    }
};
