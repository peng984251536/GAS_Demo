#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Components/Widget.h"

UCC_UIController::UCC_UIController() { ModelClass = UCC_UIModel::StaticClass(); }

UWorld* UCC_UIController::GetWorld() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && View.IsValid() ? View->GetWorld() : nullptr;
}

void UCC_UIController::Initialize(UWidget* InView, UObject* InContext)
{
	if (View.IsValid() || !InView) return;
	View = InView;
	Context = InContext;
	if (ModelClass && !ModelClass->HasAnyClassFlags(CLASS_Abstract)) Model = NewObject<UCC_UIModel>(this, ModelClass);
}

UCC_UIController* UCC_UIController::CreateForView(UWidget* InView, TSubclassOf<UCC_UIController> ControllerClass, UObject* InContext)
{
	if (!InView || !ControllerClass || ControllerClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
	UCC_UIController* Controller = NewObject<UCC_UIController>(InView, ControllerClass);
	Controller->Initialize(InView, InContext);
	return Controller;
}

UCC_ActivatableWidget* UCC_UIController::GetScreenView() const { return Cast<UCC_ActivatableWidget>(View.Get()); }

void UCC_UIController::Activate()
{
	if (bActive || !View.IsValid() || !Model) return;
	bActive = true;
	ActivationToken = FGuid::NewGuid();
	OnActivated();
}

void UCC_UIController::Deactivate()
{
	if (!bActive) return;
	bActive = false;
	ActivationToken.Invalidate();
	OnDeactivated();
}

void UCC_UIController::UpdateContext(UObject* InContext)
{
	Context = InContext;
	if (!bActive) return; // 被覆盖的页面在恢复激活时自然读取新 Context。
	// 结束旧会话：旧 ActivationToken 失效，迟到的异步结果会被拒绝。
	Deactivate();
	Activate();
}
void UCC_UIController::Release()
{
	Deactivate();
	Context = nullptr;
	View.Reset();
}

APlayerController* UCC_UIController::GetPlayerController() const { return View.IsValid() ? View->GetOwningPlayer() : nullptr; }

UCC_RootLayout* UCC_UIController::GetRootLayout() const
{
	return UCC_UIManagerSubsystem::GetRootLayoutForPlayer(GetPlayerController());
}

bool UCC_UIController::CanHandleActions() const
{
	// 只有入栈页面有"栈顶可交互"的概念；世界覆盖层等常驻视图不处理用户操作。
	const UCC_ActivatableWidget* Screen = GetScreenView();
	const UCC_RootLayout* Root = GetRootLayout();
	return bActive && Screen && Screen->IsActivated() && Root && !Root->IsShuttingDown() && !Root->IsInputBlocked() && Root->GetTopScreen() == Screen;
}

bool UCC_UIController::RequestClose() { return CanHandleActions() && GetScreenView()->CloseScreen(); }
void UCC_UIController::OnActivated() { ReceiveActivated(); }
void UCC_UIController::OnDeactivated() { ReceiveDeactivated(); }
