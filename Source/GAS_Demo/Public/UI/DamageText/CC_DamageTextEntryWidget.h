#pragma once

#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextEntryWidget.generated.h"

class UTextBlock;

/**
 * 同一个数字 UMG 模板的原生基类：不同颜色通过数据切换，不为每种伤害创建不同控件类。
 * 蓝图可使用名为 DamageText 的 TextBlock，自动接收文字、字体和颜色；不需要 Canvas。
 * 没有蓝图设计树时创建原生 TextBlock，保证默认数字可以显示。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_DamageTextEntryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /**
     * 配置共享字体和只显示、不参与鼠标命中测试的数字控件。
     * @param ObjectInitializer 引擎提供的对象初始化参数。
     */
    UCC_DamageTextEntryWidget(const FObjectInitializer& ObjectInitializer);

    /**
     * 更新显示数据和简单颜色；只在数值/样式变化或首次取得控件时调用。
     * @param InAmount 显示数值，格式化时取绝对值；治疗带加号。
     * @param InStyle 普通、暴击或治疗样式。
     * @param InDamageType 分类标签，可供蓝图选择额外样式。
     * @param InFontSize 公共容器指定的字号，不更换字体资产或创建独立材质。
     */
    void UpdateDamageText(float InAmount, ECC_DamageTextStyle InStyle, FGameplayTag InDamageType, int32 InFontSize);

    /** 统一字体资产和材质；红蓝颜色变化不通过独立动态材质实现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text|Style") FSlateFontInfo Font;
    /** 普通伤害与暴击默认红色，暴击可使用较大字号。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text|Style") FLinearColor DamageColor = FLinearColor(1.f, 0.12f, 0.12f);
    /** 治疗默认蓝色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text|Style") FLinearColor HealingColor = FLinearColor(0.15f, 0.5f, 1.f);
    UPROPERTY(BlueprintReadOnly, Category="Damage Text|Data") float Amount = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Damage Text|Data") ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal;
    UPROPERTY(BlueprintReadOnly, Category="Damage Text|Data") FGameplayTag DamageType;
    UPROPERTY(BlueprintReadOnly, Category="Damage Text|Data") FText DisplayText;
protected:
    /**
     * 设计树为空时创建一个 TextBlock；已有蓝图布局时保持其结构。
     */
    virtual void NativeOnInitialized() override;

    /**
     * 蓝图更新入口；可以设置自己的文字、图标或内部动画，不需要逐帧绑定文本属性。
     * @param InAmount 当前显示数值。
     * @param InStyle 普通、暴击或治疗样式。
     * @param InDamageType 当前伤害分类。
     */
    UFUNCTION(BlueprintImplementableEvent, Category="Damage Text", meta=(DisplayName="On Damage Text Updated"))
    void OnDamageTextUpdated(float InAmount, ECC_DamageTextStyle InStyle, FGameplayTag InDamageType);

    /** 蓝图 TextBlock 命名为 DamageText 时自动绑定；不同布局也可通过更新事件自行显示。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DamageText;
};
