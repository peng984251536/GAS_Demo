#include "UI/Combat/CC_CombatMainWidget.h"
#include "UI/Combat/CC_CombatMainController.h"
#include "UI/Combat/CC_PlayerVitalsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

// 常驻战斗界面只展示游戏数据，不消费返回键或参与菜单焦点竞争。
UCC_CombatMainWidget::UCC_CombatMainWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_CombatMainController::StaticClass();
	InputConfig = ECC_UIInputMode::Game;
	bIsBackHandler = false;
	bAllowBack = false;
	bAllowClose = false;
	bSupportsActivationFocus = false;
}

// 蓝图有布局时保留其布局，否则构建最小示例：左上角放一个血蓝条子控件。
void UCC_CombatMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree->RootWidget)
		return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	PlayerVitals = WidgetTree->ConstructWidget<UCC_PlayerVitalsWidget>(UCC_PlayerVitalsWidget::StaticClass(), TEXT("PlayerVitals"));
	UCanvasPanelSlot* VitalsSlot = Canvas->AddChildToCanvas(PlayerVitals);
	VitalsSlot->SetPosition(FVector2D(32.f, 32.f));
	VitalsSlot->SetSize(FVector2D(320.f, 100.f));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 展示层只读取模型并分发给子控件，不查找角色或订阅 GAS。
void UCC_CombatMainWidget::NativeOnModelChanged()
{
	if (const UCC_CombatMainModel* Model = Cast<UCC_CombatMainModel>(GetUIModel()))
	{
		const FCC_PlayerVitals Value = Model->GetVitals();
		if (PlayerVitals) PlayerVitals->SetVitals(Value);
		OnVitalsChanged(Value.Health, Value.MaxHealth, Value.Mana, Value.MaxMana);
	}
	Super::NativeOnModelChanged();
}
