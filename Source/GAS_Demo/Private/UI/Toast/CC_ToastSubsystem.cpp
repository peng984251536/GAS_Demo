#include "UI/Toast/CC_ToastSubsystem.h"
#include "UI/Toast/CC_ToastHostWidget.h"
#include "UI/Toast/CC_ToastEntryWidget.h"
#include "UI/Toast/CC_ToastSettings.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

bool UCC_ToastSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UCC_ToastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void UCC_ToastSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Pending.Reset();
	Host.Reset();
	Super::Deinitialize();
}

UCC_ToastSubsystem* UCC_ToastSubsystem::Get(const APlayerController* Player)
{
	const ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UCC_ToastSubsystem>() : nullptr;
}

bool UCC_ToastSubsystem::ShowToast(const FCC_ToastRequest& Request)
{
	if (Request.Title.IsEmpty() && Request.Message.IsEmpty())
	{
		return false;
	}
	if (UCC_ToastHostWidget* CurrentHost = EnsureHost())
	{
		CurrentHost->Enqueue(Request);
	}
	else
	{
		// 根布局还没挂上（刚进图/切图中）：先缓存，Ticker 里等它就绪。
		const int32 MaxPending = GetDefault<UCC_ToastSettings>()->MaxPendingBeforeLayout;
		if (MaxPending <= 0) return true;
		if (Pending.Num() >= MaxPending) Pending.RemoveAt(0);
		Pending.Add(Request);
	}
	EnsureTicking();
	return true;
}

void UCC_ToastSubsystem::ClearToasts()
{
	Pending.Reset();
	if (UCC_ToastHostWidget* CurrentHost = Host.Get())
	{
		CurrentHost->ClearAll();
	}
}

UCC_ToastHostWidget* UCC_ToastSubsystem::EnsureHost()
{
	// 宿主仍在通知层里就复用；被切图清掉（没有父控件）就重建。
	if (UCC_ToastHostWidget* CurrentHost = Host.Get())
	{
		if (CurrentHost->GetParent()) return CurrentHost;
		Host.Reset();
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* Player = LocalPlayer ? LocalPlayer->PlayerController.Get() : nullptr;
	UCC_RootLayout* Root = UCC_UIManagerSubsystem::GetRootLayoutForPlayer(Player);
	if (!Root || !Root->IsLayoutAttached() || Root->IsShuttingDown())
	{
		return nullptr;
	}

	const UCC_ToastSettings* Settings = GetDefault<UCC_ToastSettings>();
	TSubclassOf<UCC_ToastHostWidget> HostClass = Settings->HostWidgetClass.LoadSynchronous();
	if (!HostClass)
	{
		HostClass = UCC_ToastHostWidget::StaticClass();
	}
	// Duration=0：宿主常驻，由本子系统管理条目的计时。
	UCC_ToastHostWidget* NewHost = Cast<UCC_ToastHostWidget>(Root->ShowNotification(HostClass, 0.f));
	if (NewHost)
	{
		NewHost->SetHoldWhileMenuOpen(Settings->bHoldWhileMenuOpen);
		NewHost->SetEntryClassOverride(Settings->EntryWidgetClass.LoadSynchronous());
		Host = NewHost;
	}
	return NewHost;
}

void UCC_ToastSubsystem::EnsureTicking()
{
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
	}
}

bool UCC_ToastSubsystem::Tick(float DeltaTime)
{
	UCC_ToastHostWidget* CurrentHost = EnsureHost();
	if (CurrentHost && !Pending.IsEmpty())
	{
		for (const FCC_ToastRequest& Request : Pending) CurrentHost->Enqueue(Request);
		Pending.Reset();
	}
	if (CurrentHost)
	{
		// 卡顿帧不要让提示一下子跳完。
		CurrentHost->Advance(FMath::Min(DeltaTime, 0.1f));
	}
	const bool bKeepTicking = !Pending.IsEmpty() || (CurrentHost && CurrentHost->HasWork());
	if (!bKeepTicking)
	{
		TickHandle.Reset();
	}
	return bKeepTicking;
}

void UCC_ToastSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// 只处理本玩家所在的游戏世界；旧图的提示不带到新图。
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!World || !LocalPlayer || World != LocalPlayer->GetWorld()) return;
	ClearToasts();
	Host.Reset();
}
