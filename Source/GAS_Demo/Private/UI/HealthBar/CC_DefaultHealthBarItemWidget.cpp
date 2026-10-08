#include "UI/HealthBar/CC_DefaultHealthBarItemWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"

UCC_DefaultHealthBarItemWidget::UCC_DefaultHealthBarItemWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 头顶血条只显示，不拦截点击。
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 布局：Border(边框) → SizeBox(血条尺寸) → Overlay[ 背景 Image, 左对齐 SizeBox → 填充 Image ]。
void UCC_DefaultHealthBarItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 派生蓝图有自己的设计树时保留它，由蓝图覆盖 On Health Bar Updated 自行刷新。
	if (WidgetTree->RootWidget) return;
	Frame = WidgetTree->ConstructWidget<UBorder>();
	WidgetTree->RootWidget = Frame;
	BarBox = WidgetTree->ConstructWidget<USizeBox>();
	Frame->SetContent(BarBox);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	BarBox->SetContent(Layers);
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetColorAndOpacity(BackgroundColor);
	if (UOverlaySlot* BackgroundSlot = Layers->AddChildToOverlay(Background))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}
	FillBox = WidgetTree->ConstructWidget<USizeBox>();
	Fill = WidgetTree->ConstructWidget<UImage>();
	FillBox->SetContent(Fill);
	if (UOverlaySlot* FillSlot = Layers->AddChildToOverlay(FillBox))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UCC_DefaultHealthBarItemWidget::OnHealthBarUpdated_Implementation(const FCC_HealthBarItemData& Data)
{
	if (!Frame || !BarBox || !FillBox || !Fill) return;
	const float Border = FMath::IsFinite(BorderWidth) ? FMath::Max(0.f, BorderWidth) : 0.f;
	Frame->SetBrushColor(Border > 0 ? BorderColor : FLinearColor::Transparent);
	Frame->SetPadding(FMargin(Border));
	BarBox->SetWidthOverride(Data.Size.X);
	BarBox->SetHeightOverride(Data.Size.Y);
	const float Fraction = FMath::Clamp(Data.Fraction, 0.f, 1.f);
	FillBox->SetWidthOverride(Data.Size.X * Fraction);
	Fill->SetColorAndOpacity(Data.Color);
	Fill->SetVisibility(Fraction > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
