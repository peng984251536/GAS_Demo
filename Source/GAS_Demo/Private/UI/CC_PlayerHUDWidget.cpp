#include "UI/CC_PlayerHUDWidget.h"
#include "UI/HUD/CC_PlayerHUDController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

// 常驻 HUD 只展示游戏数据，不消费返回键或参与菜单焦点竞争。
UCC_PlayerHUDWidget::UCC_PlayerHUDWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_PlayerHUDController::StaticClass();
	InputConfig = ECC_UIInputMode::Game;
	bIsBackHandler = false;
	bAllowBack = false;
	bAllowClose = false;
	bSupportsActivationFocus = false;
}

// 蓝图有布局时保留其布局，否则构建最小血蓝条示例。
void UCC_PlayerHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 蓝图通过 On Vitals Changed 更新自己的控件，不依赖下面的原生示例布局。
	if (WidgetTree->RootWidget)
		return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* StatsSlot = Canvas->AddChildToCanvas(Stats);
	StatsSlot->SetPosition(FVector2D(32.f, 32.f));
	StatsSlot->SetSize(FVector2D(320.f, 100.f));
	VitalsText = WidgetTree->ConstructWidget<UTextBlock>();
	Stats->AddChildToVerticalBox(VitalsText)->SetPadding(FMargin(0, 0, 0, 8));
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.25f, 0.75f, 0.32f));
	Stats->AddChildToVerticalBox(HealthBar)->SetPadding(FMargin(0, 0, 0, 8));
	ManaBar = WidgetTree->ConstructWidget<UProgressBar>();
	ManaBar->SetFillColorAndOpacity(FLinearColor(0.18f, 0.5f, 0.95f));
	Stats->AddChildToVerticalBox(ManaBar);
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 展示层只读取模型，不查找角色或订阅 GAS。
void UCC_PlayerHUDWidget::NativeOnModelChanged()
{
	if (const UCC_PlayerHUDModel* Model = Cast<UCC_PlayerHUDModel>(GetUIModel()))
	{
		const FCC_PlayerVitals Value = Model->GetVitals();
		if (HealthBar) HealthBar->SetPercent(Value.MaxHealth > 0
			                                     ? FMath::Clamp(Value.Health / Value.MaxHealth, 0.f, 1.f)
			                                     : 0.f);
		if (ManaBar) ManaBar->SetPercent(Value.MaxMana > 0 ? FMath::Clamp(Value.Mana / Value.MaxMana, 0.f, 1.f) : 0.f);
		if (VitalsText) VitalsText->SetText(FText::FromString(FString::Printf(
			TEXT("HP %.0f / %.0f    MP %.0f / %.0f"), Value.Health, Value.MaxHealth, Value.Mana, Value.MaxMana)));
		OnVitalsChanged(Value.Health, Value.MaxHealth, Value.Mana, Value.MaxMana);
	}
	Super::NativeOnModelChanged();
}
