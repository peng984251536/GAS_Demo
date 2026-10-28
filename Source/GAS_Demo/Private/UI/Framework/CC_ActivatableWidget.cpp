#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "CommonButtonBase.h"

// 初始化页面默认策略：支持返回及焦点恢复。
UCC_ActivatableWidget::UCC_ActivatableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bAutoRestoreFocus = true;
}

// 按输入模式补齐鼠标捕获与移动/视角屏蔽，组成官方输入配置。
TOptional<FUIInputConfig> UCC_ActivatableWidget::GetDesiredInputConfig() const
{
	// 与引擎 FInputModeGameAndUI 一致：混合模式仅在按下时捕获，
	// 既让 HUD 按钮正常接收首击，也保留未被 UI 消费的地面点击。
	const EMouseCaptureMode CaptureMode = InputMode == ECommonInputMode::Menu
		                                      ? EMouseCaptureMode::NoCapture
		                                      : (InputMode == ECommonInputMode::All
			                                         ? EMouseCaptureMode::CaptureDuringMouseDown
			                                         : GameMouseCaptureMode);

	FUIInputConfig Config(InputMode, CaptureMode, false);
	Config.bIgnoreMoveInput = InputMode == ECommonInputMode::Menu;
	Config.bIgnoreLookInput = InputMode == ECommonInputMode::Menu;

	return Config;
}

// 绑定可选关闭按钮；不允许关闭的页面（常驻 HUD）直接折叠它。
void UCC_ActivatableWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (CommonButtonBase_Close)
	{
		CommonButtonBase_Close->OnClicked().AddUObject(this, &ThisClass::HandleCloseButtonClicked);
		if (!bAllowClose) CommonButtonBase_Close->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// 与返回键同一流程，蓝图可在 On Handle Back Action 里拦截（例如未保存提示）。
void UCC_ActivatableWidget::NativeOnCloseButtonClicked()
{
	if (bAllowClose && !BP_OnHandleBackAction()) CloseScreen();
}

// 页面每次重建 Slate 时重新设置导航边界。
void UCC_ActivatableWidget::NativeConstruct()
{
	Super::NativeConstruct();
	for (EUINavigation Direction : {
		     EUINavigation::Left, EUINavigation::Right, EUINavigation::Up,
		     EUINavigation::Down, EUINavigation::Next, EUINavigation::Previous
	     })
	{
		SetNavigationRuleBase(Direction, EUINavigationRule::Stop);
	}
}

// 先问蓝图/官方默认焦点，再按项目配置的控件名称查找。
UWidget* UCC_ActivatableWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget()) return Target;
	return WidgetTree && !DefaultFocusWidgetName.IsNone() ? WidgetTree->FindWidget(DefaultFocusWidgetName) : nullptr;
}

// 通过所属控制器找到根布局，统一校验后关闭。
bool UCC_ActivatableWidget::CloseScreen()
{
	if (UCC_RootLayout* Root = UCC_UIManagerSubsystem::GetRootLayoutForPlayer(GetOwningPlayer())) return Root->
		CloseScreen(this);
	return false;
}

// 蓝图可优先消费返回事件，未处理时走框架关闭；禁止输入泄漏给游戏。
bool UCC_ActivatableWidget::NativeOnHandleBackAction()
{
	// 即使底页不允许关闭，也消费返回键，避免继续传给游戏。
	if (bAllowBack && !BP_OnHandleBackAction()) CloseScreen();
	return true;
}

// 每次打开都更新数据上下文，避免对象池中旧数据和旧焦点影响新页面。
void UCC_ActivatableWidget::PrepareForDisplay(UObject* InContext)
{
	// 官方容器可能暂留上一帧的 Slate 引用；每次重新入栈主动结束旧会话。
	if (UCC_UIModel* Model = GetUIModel()) Model->OnChanged.RemoveDynamic(this, &ThisClass::HandleModelChanged);
	if (ScreenController) ScreenController->Release();
	ScreenController = nullptr;
	ScreenContext = InContext;
	EnsureController();
	ClearFocusRestorationTarget();
	BP_OnScreenOpened(InContext);
}

// 复用已在栈中的实例：只换数据，不重建会话对象，避免模型状态（如滚动位置之外的业务快照）被无谓清空。
void UCC_ActivatableWidget::ReuseWithContext(UObject* InContext)
{
	if (!InContext || InContext == ScreenContext) return;
	ScreenContext = InContext;
	if (ScreenController) ScreenController->UpdateContext(InContext);
	BP_OnScreenOpened(InContext);
}

// 控件释放 Slate 时清除上下文，避免对象池继续持有旧业务对象。
void UCC_ActivatableWidget::NativeDestruct()
{
	if (ScreenController) ScreenController->Deactivate();
	if (UCC_UIModel* Model = GetUIModel()) Model->OnChanged.RemoveDynamic(this, &ThisClass::HandleModelChanged);
	Super::NativeDestruct();
	if (ScreenController) ScreenController->Release();
	ScreenController = nullptr;
	// 页面返回 CommonUI 对象池时释放旧业务上下文；失活但仍在栈中的页面会保留上下文。
	ScreenContext = nullptr;
}

void UCC_ActivatableWidget::EnsureController()
{
	if (!ScreenController && ControllerClass && !ControllerClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ScreenController = NewObject<UCC_UIController>(this, ControllerClass);
		ScreenController->Initialize(this, ScreenContext);
	}
}

UCC_UIModel* UCC_ActivatableWidget::GetUIModel() const
{
	return ScreenController ? ScreenController->GetModel() : nullptr;
}

void UCC_ActivatableWidget::NativeOnActivated()
{
	EnsureController();
	// 先建立展示订阅，再启动数据源，确保首次同步事件不会丢失。
	if (UCC_UIModel* Model = GetUIModel()) Model->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleModelChanged);
	if (ScreenController) ScreenController->Activate();
	HandleModelChanged();
	Super::NativeOnActivated();
}

void UCC_ActivatableWidget::NativeOnDeactivated()
{
	if (UCC_UIModel* Model = GetUIModel())
		Model->OnChanged.RemoveDynamic(this, &ThisClass::HandleModelChanged);
	
	if (ScreenController)
		ScreenController->Deactivate();
	
	Super::NativeOnDeactivated();
}

void UCC_ActivatableWidget::HandleModelChanged()
{
	if (IsActivated() && GetUIModel()) NativeOnModelChanged();
}

void UCC_ActivatableWidget::NativeOnModelChanged()
{
	OnUIModelChanged(GetUIModel());
}
