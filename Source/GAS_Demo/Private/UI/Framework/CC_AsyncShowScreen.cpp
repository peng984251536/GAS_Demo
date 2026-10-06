#include "UI/Framework/CC_AsyncShowScreen.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UCC_AsyncShowScreen* UCC_AsyncShowScreen::ShowScreenAsync(UCC_RootLayout* RootLayout, FGameplayTag Layer,
	TSoftClassPtr<UCC_ActivatableWidget> ScreenClass, UObject* Context, bool bSuspendInput)
{
	UCC_AsyncShowScreen* Action = NewObject<UCC_AsyncShowScreen>();
	Action->Root = RootLayout;
	Action->TargetLayer = Layer;
	Action->ClassToLoad = ScreenClass;
	Action->DisplayContext = Context;
	Action->bSuspend = bSuspendInput;
	if (RootLayout) Action->RegisterWithGameInstance(RootLayout);
	return Action;
}

void UCC_AsyncShowScreen::Activate()
{
	if (bStarted || bFinished) return;
	bStarted = true;
	UCC_RootLayout* Layout = Root.Get();
	if (!Layout || Layout->IsShuttingDown() || !Layout->IsLayoutAttached() || !Layout->GetLayer(TargetLayer) || ClassToLoad.IsNull())
	{
		Finish(nullptr);
		return;
	}
	ShutdownHandle = Layout->OnShutdown.AddUObject(this, &ThisClass::Cancel);
	if (bSuspend) InputToken = Layout->SuspendAsyncInput();
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ClassToLoad.ToSoftObjectPath());
	if (!LoadHandle.IsValid()) { Finish(nullptr); return; }
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickRequest));
}

bool UCC_AsyncShowScreen::TickRequest(float)
{
	UCC_RootLayout* Layout = Root.Get();
	if (bFinished) return false;
	if (!Layout || Layout->IsShuttingDown() || !Layout->GetOwningPlayer()) { Cancel(); return false; }
	if (!LoadHandle->HasLoadCompleted()) return true;
	UClass* Class = ClassToLoad.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || !Class->IsChildOf(UCC_ActivatableWidget::StaticClass()))
	{
		Finish(nullptr);
		return false;
	}
	if (!Layout->CanAcceptScreen()) return true;
	Finish(Layout->ShowScreen(TargetLayer, Class, DisplayContext));
	return false;
}

void UCC_AsyncShowScreen::Finish(UCC_ActivatableWidget* Screen)
{
	if (bFinished) return;
	bFinished = true;
	Cleanup();
	if (Screen) Completed.Broadcast(Screen);
	else Failed.Broadcast(nullptr);
	SetReadyToDestroy();
}

void UCC_AsyncShowScreen::Cancel()
{
	if (!bFinished)
	{
		bFinished = true;
		Cleanup();
	}
	Super::Cancel();
}

void UCC_AsyncShowScreen::Cleanup()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();
	if (UCC_RootLayout* Layout = Root.Get())
	{
		Layout->OnShutdown.Remove(ShutdownHandle);
		Layout->ResumeAsyncInput(InputToken);
	}
	InputToken = NAME_None;
	if (LoadHandle && !LoadHandle->HasLoadCompleted()) LoadHandle->CancelHandle();
	LoadHandle.Reset();
	DisplayContext = nullptr;
}
