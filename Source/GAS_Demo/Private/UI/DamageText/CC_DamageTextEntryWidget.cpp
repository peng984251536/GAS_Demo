#include "UI/DamageText/CC_DamageTextEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

/**
 * 配置可复用的纯显示数字；不编写独立 Tick，不为红蓝样式创建不同材质。
 * @param ObjectInitializer 引擎提供的对象初始化参数。
 */
UCC_DamageTextEntryWidget::UCC_DamageTextEntryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Font = FCoreStyle::GetDefaultFontStyle("Bold", 28);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

/**
 * 保留蓝图设计树；没有设计树时提供一个无嵌套 Canvas 的默认 TextBlock。
 */
void UCC_DamageTextEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageText"));
        WidgetTree->RootWidget = DamageText;
        DamageText->SetJustification(ETextJustify::Center);
    }
    if (DamageText) DamageText->SetFont(Font);
}

/**
 * 更新文字和顶点颜色，再通知蓝图；容器另行统一处理位置、透明度、缩放与回收。
 * @param InAmount 当前显示数值。
 * @param InStyle 普通、暴击或治疗样式。
 * @param InDamageType 当前伤害分类。
 * @param InFontSize 公共容器提供的字号。
 */
void UCC_DamageTextEntryWidget::UpdateDamageText(float InAmount, ECC_DamageTextStyle InStyle,
    FGameplayTag InDamageType, int32 InFontSize)
{
    Amount = InAmount;
    Style = InStyle;
    DamageType = InDamageType;
    Font.Size = FMath::Clamp(InFontSize, 1, 512);
    FNumberFormattingOptions Options;
    Options.SetUseGrouping(false);
    Options.SetMinimumFractionalDigits(0);
    Options.SetMaximumFractionalDigits(0);
    const FText Number = FText::AsNumber(static_cast<double>(FMath::Abs(Amount)), &Options);
    DisplayText = Style == ECC_DamageTextStyle::Heal
        ? FText::Format(NSLOCTEXT("DamageText", "HealFormat", "+{0}"), Number) : Number;
    if (DamageText)
    {
        DamageText->SetText(DisplayText);
        DamageText->SetFont(Font);
        DamageText->SetColorAndOpacity(FSlateColor(Style == ECC_DamageTextStyle::Heal ? HealingColor : DamageColor));
    }
    OnDamageTextUpdated(Amount, Style, DamageType);
}
