#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "GameplayTags/CC_Tags.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// 把控件铺满 Overlay 插槽，层级由添加顺序决定。
	void Fill(UOverlay* Overlay, UWidget* Widget)
	{
		UOverlaySlot* Slot = Overlay->AddChildToOverlay(Widget);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}
	// 给键鼠、手柄、触摸登记相同原因的输入过滤，关闭时按相同令牌解除。
	void FilterInput(ULocalPlayer* Player, FName Token, bool bFilter)
	{
		if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(Player))
		{
			for (ECommonInputType Type : {ECommonInputType::MouseAndKeyboard, ECommonInputType::Gamepad, ECommonInputType::Touch})
				Input->SetInputTypeFilter(Type, Token, bFilter);
		}
	}
}

// 配置官方动画、回退策略及切图时对象池释放方式。
void UCC_UIStack::Configure(ECommonSwitcherTransition Type, float Duration)
{
	TransitionType = Type;
	TransitionCurveType = ETransitionCurve::CubicInOut;
	TransitionFallbackStrategy = ECommonSwitcherTransitionFallbackStrategy::Previous;
	bResetPoolWhenReleasingSlateResources = true;
	SetTransitionDuration(Duration);
}

// 根布局默认激活且自身不拦截子控件的点击。
UCC_RootLayout::UCC_RootLayout(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAutoActivate = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// 根布局提供稳定的游戏输入回退，避免最后一个菜单关闭后卡在 UI 模式。
TOptional<FUIInputConfig> UCC_RootLayout::GetDesiredInputConfig() const
{
	//TODO - ui可以阻断输入映射
	// 根布局始终作为游戏输入回退，避免关闭最后一页后仍停在 UI-only 模式。
	return FUIInputConfig(ECommonInputMode::Game,
		EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
		false);
}

UWorld* UCC_RootLayout::GetWorld() const
{
	const UGameInstance* Instance = GetTypedOuter<UGameInstance>();
	return Instance ? Instance->GetWorld() : Super::GetWorld();
}

// 按视觉顺序构建页面容器、动画遮罩及不抢输入的提示层。
void UCC_RootLayout::NativeOnInitialized()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	GameStack = WidgetTree->ConstructWidget<UCC_UIStack>(UCC_UIStack::StaticClass(), TEXT("Game"));
	GameMenuStack = WidgetTree->ConstructWidget<UCC_UIStack>(UCC_UIStack::StaticClass(), TEXT("GameMenu"));
	MenuStack = WidgetTree->ConstructWidget<UCC_UIStack>(UCC_UIStack::StaticClass(), TEXT("Menu"));
	ModalStack = WidgetTree->ConstructWidget<UCC_UIStack>(UCC_UIStack::StaticClass(), TEXT("Modal"));
	GameStack->Configure(ECommonSwitcherTransition::FadeOnly, 0.f);
	GameMenuStack->Configure(MenuTransition, TransitionDuration);
	MenuStack->Configure(MenuTransition, TransitionDuration);
	ModalStack->Configure(ModalTransition, TransitionDuration);
	for (UCC_UIStack* Stack : {GameStack.Get(), GameMenuStack.Get(), MenuStack.Get(), ModalStack.Get()})
	{
		Fill(Root, Stack);
		Stack->OnTransitioningChanged.AddUObject(this, &ThisClass::HandleTransition);
	}
	GameStack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayed, CCTags::UILayer::Game);
	GameMenuStack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayed, CCTags::UILayer::GameMenu);
	MenuStack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayed, CCTags::UILayer::Menu);
	ModalStack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayed, CCTags::UILayer::Modal);
	UBorder* Shield = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TransitionInputShield"));
	Shield->SetBrushColor(FLinearColor::Transparent);
	Shield->SetVisibility(ESlateVisibility::Collapsed);
	Fill(Root, Shield);
	InputShield = Shield;
	NotificationLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Notifications"));
	NotificationLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Root, NotificationLayer);
	Super::NativeOnInitialized();
}

// GameMenu 和 Menu 是不同的物理栈；打开设置不会替换背包所在栈。
UCommonActivatableWidgetContainerBase* UCC_RootLayout::GetLayer(FGameplayTag Layer) const
{
	if (Layer == CCTags::UILayer::Game) return GameStack;
	if (Layer == CCTags::UILayer::GameMenu) return GameMenuStack;
	if (Layer == CCTags::UILayer::Menu) return MenuStack;
	if (Layer == CCTags::UILayer::Modal) return ModalStack;
	return nullptr;
}

// 检查层级、重复页面和重入，再用官方入栈前回调注入上下文。
UCC_ActivatableWidget* UCC_RootLayout::ShowScreen(FGameplayTag Layer, TSubclassOf<UCC_ActivatableWidget> ScreenClass, UObject* Context)
{
	UCommonActivatableWidgetContainerBase* Stack = GetLayer(Layer);
	if (bShuttingDown || bDetached || bChangingStack || !Stack || !ScreenClass || ScreenClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
	// 同层同类只保留一个实例；已被覆盖的旧页面不会被悄悄移到栈顶。
	for (UCommonActivatableWidget* Existing : Stack->GetWidgetList())
		if (Existing->GetClass() == ScreenClass) return Cast<UCC_ActivatableWidget>(Existing);
	if (IsTransitioning()) return nullptr;
	// 作用域结束自动解除重入锁，保证提前返回也不会卡住后续操作。
	TGuardValue<bool> ChangingStack(bChangingStack, true);
	UCC_ActivatableWidget* Result = Stack->AddWidget<UCC_ActivatableWidget>(ScreenClass,
		[Context](UCC_ActivatableWidget& Screen) { Screen.PrepareForDisplay(Context); });
	UpdateInteraction();
	return Result;
}

// 仅允许关闭全局最上层页面，通过失活触发官方退场流程。
bool UCC_RootLayout::CloseScreen(UCC_ActivatableWidget* Screen)
{
	if (bShuttingDown || bChangingStack || IsInputBlocked() || !IsValid(Screen) || !Screen->CanCloseScreen() || GetTopScreen() != Screen) return false;
	TGuardValue<bool> ChangingStack(bChangingStack, true);
	Screen->DeactivateWidget(); // 由官方栈退场、回收控件并恢复下页。
	return true;
}

// 对当前最高页面应用统一关闭规则。
bool UCC_RootLayout::CloseTopScreen() { return CloseScreen(Cast<UCC_ActivatableWidget>(GetTopScreen())); }
// 按 Modal、Menu、GameMenu、Game 的视觉顺序寻找当前显示页面。
UCommonActivatableWidget* UCC_RootLayout::GetTopScreen() const
{
	if (ModalStack && ModalStack->GetActiveWidget()) return ModalStack->GetActiveWidget();
	if (MenuStack && MenuStack->GetActiveWidget()) return MenuStack->GetActiveWidget();
	if (GameMenuStack && GameMenuStack->GetActiveWidget()) return GameMenuStack->GetActiveWidget();
	return GameStack ? GameStack->GetActiveWidget() : nullptr;
}
// 按栈内容判断是否有菜单，入场和退场中的实例也计算在内。
bool UCC_RootLayout::HasMenu() const
{
	return (GameMenuStack && GameMenuStack->GetNumWidgets() > 0) || (MenuStack && MenuStack->GetNumWidgets() > 0) || (ModalStack && ModalStack->GetNumWidgets() > 0);
}

// 动画开始登记输入令牌，结束解除相同令牌并刷新焦点。
void UCC_RootLayout::HandleTransition(UCommonActivatableWidgetContainerBase* Stack, bool bTransitioning)
{
	if (bShuttingDown) return;
	if (bTransitioning && !TransitionTokens.Contains(Stack))
	{
		// 每个根布局/容器使用独立令牌，多个过渡不会互相提前解锁。
		const FName Token(*FString::Printf(TEXT("CCUI_%u_%u"), GetUniqueID(), Stack->GetUniqueID()));
		TransitionTokens.Add(Stack, Token);
		FilterInput(GetOwningLocalPlayer(), Token, true);
	}
	else if (!bTransitioning)
	{
		FName Token;
		if (TransitionTokens.RemoveAndCopyValue(Stack, Token)) FilterInput(GetOwningLocalPlayer(), Token, false);
	}
	UpdateInteraction();
	if (!IsTransitioning())
		if (UCC_ActivatableWidget* Top = Cast<UCC_ActivatableWidget>(GetTopScreen())) Top->RefreshNavigationFocus();
}

// 官方栈切换显示页面后同步交互，并通知外部订阅者。
void UCC_RootLayout::HandleDisplayed(UCommonActivatableWidget* Screen, FGameplayTag Layer)
{
	if (bShuttingDown) return;
	UpdateInteraction();
	OnLayerChanged.Broadcast(Layer, Screen);
}

// 阻止下层被点击，并同步动画遮罩与单机暂停状态。
void UCC_RootLayout::UpdateInteraction()
{
	if (!GameStack || !GameMenuStack || !MenuStack || !ModalStack) return;
	// 鼠标命中与输入路由分开处理：保留下层画面，但禁止它被点击。
	const bool bModal = ModalStack->GetNumWidgets() > 0;
	const bool bMenu = MenuStack->GetNumWidgets() > 0;
	const bool bGameMenu = GameMenuStack->GetNumWidgets() > 0;
	if (bPauseGameWhileMenuOpen && GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
	{
		if ((bGameMenu || bMenu || bModal) && !UGameplayStatics::IsGamePaused(this)) bPausedWorld = UGameplayStatics::SetGamePaused(this, true);
		else if (!bGameMenu && !bMenu && !bModal && !IsTransitioning() && bPausedWorld)
		{
			UGameplayStatics::SetGamePaused(this, false);
			bPausedWorld = false;
		}
	}
	if (GameStack->GetActiveWidget()) GameStack->SetVisibility(bModal || bMenu || bGameMenu ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	if (GameMenuStack->GetActiveWidget()) GameMenuStack->SetVisibility(bModal || bMenu ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	if (MenuStack->GetActiveWidget()) MenuStack->SetVisibility(bModal ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	if (InputShield) InputShield->SetVisibility(IsInputBlocked() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// 创建普通提示控件并设置可选移除计时器；提示不参与页面导航。
UUserWidget* UCC_RootLayout::ShowNotification(TSubclassOf<UUserWidget> WidgetClass, float Duration)
{
	if (bShuttingDown || !NotificationLayer || !WidgetClass || WidgetClass->IsChildOf<UCommonActivatableWidget>()) return nullptr;
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetOwningPlayer(), WidgetClass);
	if (!Widget) return nullptr;
	Fill(NotificationLayer, Widget);
	if (Duration > 0.f)
	{
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakWidget = TWeakObjectPtr<UUserWidget>(Widget)]()
		{
			DismissNotification(WeakWidget.Get());
		}), Duration, false);
		NotificationTimers.Add(Widget, Handle);
	}
	return Widget;
}

// 仅移除本提示容器中的控件，并取消它的计时器。
void UCC_RootLayout::DismissNotification(UUserWidget* Widget)
{
	if (!Widget || !NotificationLayer || !NotificationLayer->HasChild(Widget)) return;
	FTimerHandle Handle;
	if (NotificationTimers.RemoveAndCopyValue(Widget, Handle) && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Handle);
	NotificationLayer->RemoveChild(Widget);
}

// 清理本根布局施加的输入过滤，不影响其他原因的过滤。
void UCC_RootLayout::ReleaseInputTokens()
{
	for (const auto& Pair : TransitionTokens) FilterInput(GetOwningLocalPlayer(), Pair.Value, false);
	TransitionTokens.Reset();
	for (FName Token : AsyncInputTokens) FilterInput(GetOwningLocalPlayer(), Token, false);
	AsyncInputTokens.Reset();
}

// 每次加载都取得唯一令牌；遮罩同时保护鼠标点击。
FName UCC_RootLayout::SuspendAsyncInput()
{
	if (bShuttingDown) return NAME_None;
	const FName Token(*FString::Printf(TEXT("CCUI_Load_%u_%u"), GetUniqueID(), ++AsyncRequestSerial));
	AsyncInputTokens.Add(Token);
	FilterInput(GetOwningLocalPlayer(), Token, true);
	UpdateInteraction();
	return Token;
}

// 失败、成功、取消均走同一个释放入口。
void UCC_RootLayout::ResumeAsyncInput(FName Token)
{
	if (AsyncInputTokens.Remove(Token)) FilterInput(GetOwningLocalPlayer(), Token, false);
	if (!bShuttingDown) UpdateInteraction();
}

// 中止全部 UI 活动；清理可重复执行，切图期间也不会残留输入锁。
void UCC_RootLayout::ResetWorldContent()
{
	if (bShuttingDown) return;
	TGuardValue<bool> Clearing(bShuttingDown, true);
	bDetached = true;
	OnShutdown.Broadcast();
	OnShutdown.Clear();
	if (bPausedWorld) UGameplayStatics::SetGamePaused(this, false);
	bPausedWorld = false;
	ReleaseInputTokens();
	if (GetWorld()) for (auto& Pair : NotificationTimers) GetWorld()->GetTimerManager().ClearTimer(Pair.Value);
	NotificationTimers.Reset();
	if (NotificationLayer) NotificationLayer->ClearChildren();
	for (UCC_UIStack* Stack : {ModalStack.Get(), MenuStack.Get(), GameMenuStack.Get(), GameStack.Get()})
	{
		if (!Stack) continue;
		Stack->SetTransitionDuration(0.f);
		// 拷贝列表，避免失活回调修改容器时使遍历失效。
		const TArray<UCommonActivatableWidget*> Screens = Stack->GetWidgetList();
		for (UCommonActivatableWidget* Screen : Screens)
			if (IsValid(Screen) && Screen->IsActivated()) Screen->DeactivateWidget();
		Stack->ClearWidgets();
		Stack->SetTransitionDuration(Stack == GameStack ? 0.f : TransitionDuration);
	}
	DeactivateWidget();
}

// 只有玩家退出/GameInstance 结束才永久封闭根布局。
void UCC_RootLayout::Shutdown()
{
	if (bShuttingDown) return;
	ResetWorldContent();
	bShuttingDown = true;
}

// 根布局失去 Slate 时兜底清理资源。
void UCC_RootLayout::NativeDestruct()
{
	ResetWorldContent();
	Super::NativeDestruct();
}
