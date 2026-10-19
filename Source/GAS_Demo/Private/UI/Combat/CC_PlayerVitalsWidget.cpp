#include "UI/Combat/CC_PlayerVitalsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// 蓝图有布局时保留其布局（同名控件已由 BindWidgetOptional 绑定），否则构建最小血蓝条示例。
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
	VitalsText = WidgetTree->ConstructWidget<UTextBlock>();
	Stats->AddChildToVerticalBox(VitalsText)->SetPadding(FMargin(0, 0, 0, 8));
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.25f, 0.75f, 0.32f));
	Stats->AddChildToVerticalBox(HealthBar)->SetPadding(FMargin(0, 0, 0, 8));
	ManaBar = WidgetTree->ConstructWidget<UProgressBar>();
	ManaBar->SetFillColorAndOpacity(FLinearColor(0.18f, 0.5f, 0.95f));
	Stats->AddChildToVerticalBox(ManaBar);
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
	if (HealthBar) HealthBar->SetPercent(Vitals.GetHealthPercent());
	if (ManaBar) ManaBar->SetPercent(Vitals.GetManaPercent());
	if (VitalsText) VitalsText->SetText(FText::FromString(FString::Printf(
		TEXT("HP %.0f / %.0f    MP %.0f / %.0f"), Vitals.Health, Vitals.MaxHealth, Vitals.Mana, Vitals.MaxMana)));
}
