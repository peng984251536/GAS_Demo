#include "UI/Combat/CC_PlayerVitalsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// 蓝图有布局时保留其布局（同名控件已由 BindWidget 绑定），否则构建最小血蓝条示例。
void UCC_PlayerVitalsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		ApplyToWidgets();
		return;
	}
	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Stats;
	TextBlock_Vitals = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextBlock_Vitals"));
	Stats->AddChildToVerticalBox(TextBlock_Vitals)->SetPadding(FMargin(0, 0, 0, 8));
	ProgressBar_Health = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_Health"));
	ProgressBar_Health->SetFillColorAndOpacity(FLinearColor(0.25f, 0.75f, 0.32f));
	Stats->AddChildToVerticalBox(ProgressBar_Health)->SetPadding(FMargin(0, 0, 0, 8));
	ProgressBar_Mana = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_Mana"));
	ProgressBar_Mana->SetFillColorAndOpacity(FLinearColor(0.18f, 0.5f, 0.95f));
	Stats->AddChildToVerticalBox(ProgressBar_Mana);
	Stats->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyToWidgets();
}

void UCC_PlayerVitalsWidget::SetVitals(const FCC_PlayerVitals& InVitals)
{
	Vitals = InVitals;
	ApplyToWidgets();
	OnVitalsChanged(Vitals);
}

void UCC_PlayerVitalsWidget::ApplyToWidgets() const
{
	if (ProgressBar_Health) ProgressBar_Health->SetPercent(Vitals.GetHealthPercent());
	if (ProgressBar_Mana) ProgressBar_Mana->SetPercent(Vitals.GetManaPercent());
	if (TextBlock_Vitals) TextBlock_Vitals->SetText(FText::FromString(FString::Printf(
		TEXT("HP %.0f / %.0f    MP %.0f / %.0f"), Vitals.Health, Vitals.MaxHealth, Vitals.Mana, Vitals.MaxMana)));
}
