#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "GameplayTags/CC_Tags.h"
#include "GAS_Demo.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
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

// 按层级表的视觉顺序构建页面容器、动画遮罩及不抢输入的提示层。
void UCC_RootLayout::NativeOnInitialized()
{
	// Outer 铺满整个视口（放过渡遮罩），Root 承载各层和提示；开启 SafeZone 时 Root 被安全区内缩。
	UOverlay* Outer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Outer"));
	WidgetTree->RootWidget = Outer;
	Outer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	if (bApplySafeZone)
	{
		USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("SafeZone"));
		SafeZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SafeZone->AddChild(Root);
		Fill(Outer, SafeZone);
	}
	else
	{
		Fill(Outer, Root);
	}

	// 层级表留空时使用内置四层；放在这里而不是构造函数，避免 CDO 构造早于原生标签注册。
	TArray<FCC_UILayerConfig> Configs = Layers;
	if (Configs.IsEmpty())
	{
		Configs = {
			{CCTags::UILayer::Game, ECC_UILayerKind::Game},
			{CCTags::UILayer::GameMenu, ECC_UILayerKind::Menu},
			{CCTags::UILayer::Menu, ECC_UILayerKind::Menu},
			{CCTags::UILayer::Modal, ECC_UILayerKind::Modal}};
	}
	RuntimeLayers.Reset();
	for (const FCC_UILayerConfig& Config : Configs)
	{
		if (!Config.Tag.IsValid() || GetLayer(Config.Tag))
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("RootLayout: 忽略无效或重复的层标签 %s"), *Config.Tag.ToString());
			continue;
		}
		const FName StackName(*FString::Printf(TEXT("Layer_%s"), *Config.Tag.GetTagName().ToString().Replace(TEXT("."), TEXT("_"))));
		UCC_UIStack* Stack = WidgetTree->ConstructWidget<UCC_UIStack>(UCC_UIStack::StaticClass(), StackName);
		const ECommonSwitcherTransition Transition = Config.Kind == ECC_UILayerKind::Modal ? ModalTransition :
			(Config.Kind == ECC_UILayerKind::Menu ? MenuTransition : ECommonSwitcherTransition::FadeOnly);
		Stack->Configure(Transition, GetLayerDuration(Config.Kind));
		Fill(Root, Stack);
		Stack->OnTransitioningChanged.AddUObject(this, &ThisClass::HandleTransition);
		Stack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayed, Config.Tag);
		FCC_UIRuntimeLayer& Layer = RuntimeLayers.AddDefaulted_GetRef();
		Layer.Tag = Config.Tag;
		Layer.Kind = Config.Kind;
		Layer.Stack = Stack;
	}
	UBorder* Shield = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TransitionInputShield"));
	Shield->SetBrushColor(FLinearColor::Transparent);
	Shield->SetVisibility(ESlateVisibility::Collapsed);
	NotificationLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Notifications"));
	NotificationLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Root, NotificationLayer);
	// 遮罩放在 Outer 最上层，过渡期间连安全区外的边缘也拦截点击。
	Fill(Outer, Shield);
	InputShield = Shield;
	Super::NativeOnInitialized();
}

// 每个标签对应一个独立的物理栈；例如打开设置不会替换背包所在栈。
UCommonActivatableWidgetContainerBase* UCC_RootLayout::GetLayer(FGameplayTag Layer) const
{
	for (const FCC_UIRuntimeLayer& Entry : RuntimeLayers)
		if (Entry.Tag == Layer) return Entry.Stack;
	return nullptr;
}

// 检查层级、重复页面和重入，再用官方入栈前回调注入上下文。
UCC_ActivatableWidget* UCC_RootLayout::ShowScreen(FGameplayTag Layer, TSubclassOf<UCC_ActivatableWidget> ScreenClass, UObject* Context)
{
	UCommonActivatableWidgetContainerBase* Stack = GetLayer(Layer);
	if (bShuttingDown || bDetached || bChangingStack || !Stack || !ScreenClass || ScreenClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
	// 同层同类只保留一个实例；已被覆盖的旧页面不会被悄悄移到栈顶。
	// 复用时注入新的 Context，否则用不同数据（例如另一个物品）打开详情页会继续显示旧数据。
	for (UCommonActivatableWidget* Existing : Stack->GetWidgetList())
	{
		if (Existing->GetClass() != ScreenClass) continue;
		UCC_ActivatableWidget* Screen = Cast<UCC_ActivatableWidget>(Existing);
		if (Screen)
		{
			TGuardValue<bool> ChangingStack(bChangingStack, true); // On Screen Opened 中不允许同步导航。
			Screen->ReuseWithContext(Context);
		}
		if (Existing != Stack->GetActiveWidget())
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("ShowScreen: %s 已在 %s 层栈中但被同层其他页面覆盖，返回已有实例且不会移到栈顶。"),
				*GetNameSafe(ScreenClass), *Layer.ToString());
		}
		return Screen;
	}
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
// 从最高层往下寻找当前显示页面。
UCommonActivatableWidget* UCC_RootLayout::GetTopScreen() const
{
	for (int32 Index = RuntimeLayers.Num() - 1; Index >= 0; --Index)
		if (UCC_UIStack* Stack = RuntimeLayers[Index].Stack)
			if (UCommonActivatableWidget* Active = Stack->GetActiveWidget()) return Active;
	return nullptr;
}
// 按栈内容判断是否有菜单/弹窗，入场和退场中的实例也计算在内。
bool UCC_RootLayout::HasMenu() const
{
	for (const FCC_UIRuntimeLayer& Entry : RuntimeLayers)
		if (Entry.IsMenuLike() && Entry.Stack && Entry.Stack->GetNumWidgets() > 0) return true;
	return false;
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
	if (RuntimeLayers.IsEmpty()) return;
	const bool bAnyMenu = HasMenu();
	if (bPauseGameWhileMenuOpen && GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
	{
		if (bAnyMenu && !UGameplayStatics::IsGamePaused(this)) bPausedWorld = UGameplayStatics::SetGamePaused(this, true);
		else if (!bAnyMenu && !IsTransitioning() && bPausedWorld)
		{
			UGameplayStatics::SetGamePaused(this, false);
			bPausedWorld = false;
		}
	}
	// 鼠标命中与输入路由分开处理：保留下层画面，但只要上方有菜单/弹窗层持有页面，就禁止下层被点击。
	bool bBlockedByAbove = false;
	for (int32 Index = RuntimeLayers.Num() - 1; Index >= 0; --Index)
	{
		const FCC_UIRuntimeLayer& Entry = RuntimeLayers[Index];
		if (!Entry.Stack) continue;
		if (Entry.Stack->GetActiveWidget())
			Entry.Stack->SetVisibility(bBlockedByAbove ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
		if (Entry.IsMenuLike() && Entry.Stack->GetNumWidgets() > 0) bBlockedByAbove = true;
	}
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
	// 从最高层往下清空，与正常关闭顺序一致。
	for (int32 Index = RuntimeLayers.Num() - 1; Index >= 0; --Index)
	{
		UCC_UIStack* Stack = RuntimeLayers[Index].Stack;
		if (!Stack) continue;
		Stack->SetTransitionDuration(0.f);
		// 拷贝列表，避免失活回调修改容器时使遍历失效。
		const TArray<UCommonActivatableWidget*> Screens = Stack->GetWidgetList();
		for (UCommonActivatableWidget* Screen : Screens)
			if (IsValid(Screen) && Screen->IsActivated()) Screen->DeactivateWidget();
		Stack->ClearWidgets();
		Stack->SetTransitionDuration(GetLayerDuration(RuntimeLayers[Index].Kind));
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
