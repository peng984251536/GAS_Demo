#include "UI/Combat/CC_CombatMainWidget.h"
#include "UI/Combat/CC_CombatMainController.h"
#include "UI/Combat/CC_PlayerVitalsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"

// 常驻战斗界面只展示游戏数据，不消费返回键或参与菜单焦点竞争。
UCC_CombatMainWidget::UCC_CombatMainWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_CombatMainController::StaticClass();
	// HUD 上有可点击按钮，需要游戏与 UI 共同输入。
	InputMode = ECommonInputMode::All;
	bIsBackHandler = false;
	bAllowBack = false;
	bAllowClose = false;
	bSupportsActivationFocus = false;
}

// 蓝图有布局时保留其布局，否则构建最小示例：左上角放一个血蓝条子控件。
void UCC_CombatMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (CommonButtonBase_BuffSelection)
	{
		CommonButtonBase_BuffSelection->OnClicked().AddUObject(this, &ThisClass::HandleBuffSelectionClicked);
	}
	ApplyBuffRewards(0);
	if (WidgetTree->RootWidget)
		return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	PlayerVitalsWidget_PlayerVitals = WidgetTree->ConstructWidget<UCC_PlayerVitalsWidget>(UCC_PlayerVitalsWidget::StaticClass(), TEXT("PlayerVitalsWidget_PlayerVitals"));
	UCanvasPanelSlot* VitalsSlot = Canvas->AddChildToCanvas(PlayerVitalsWidget_PlayerVitals);
	VitalsSlot->SetPosition(FVector2D(32.f, 32.f));
	VitalsSlot->SetSize(FVector2D(320.f, 100.f));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UCC_CombatMainWidget::OpenBuffSelection()
{
	UCC_CombatMainController* Controller = Cast<UCC_CombatMainController>(GetScreenController());
	return Controller && Controller->OpenBuffSelection();
}

void UCC_CombatMainWidget::HandleBuffSelectionClicked()
{
	OpenBuffSelection();
}

void UCC_CombatMainWidget::ApplyBuffRewards(int32 PendingCount)
{
	const bool bHasRewards = PendingCount > 0;
	// 入口按钮始终可点（没有奖励时点击不打开页面），只用红点和次数提示；
	// 不要用 SetIsEnabled 禁用它：禁用的 Slate 控件不处理点击，点击会穿透到游戏视口触发攻击。
	if (Widget_BuffRedDot)
	{
		Widget_BuffRedDot->SetVisibility(bHasRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TextBlock_BuffRewardCount)
	{
		TextBlock_BuffRewardCount->SetText(FText::AsNumber(PendingCount));
		TextBlock_BuffRewardCount->SetVisibility(bHasRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// 展示层只读取模型并分发给子控件，不查找角色或订阅 GAS。
void UCC_CombatMainWidget::NativeOnModelChanged()
{
	if (const UCC_CombatMainModel* Model = Cast<UCC_CombatMainModel>(GetUIModel()))
	{
		const FCC_PlayerVitals Value = Model->GetVitals();
		if (PlayerVitalsWidget_PlayerVitals) PlayerVitalsWidget_PlayerVitals->SetVitals(Value);
		OnVitalsChanged(Value.Health, Value.MaxHealth, Value.Mana, Value.MaxMana);
		const int32 Rewards = Model->GetPendingBuffRewards();
		ApplyBuffRewards(Rewards);
		if (Rewards != LastBuffRewards)
		{
			LastBuffRewards = Rewards;
			OnBuffRewardsChanged(Rewards);
		}
	}
	Super::NativeOnModelChanged();
}
