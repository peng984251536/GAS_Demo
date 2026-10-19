#include "UI/WorldIndicator/CC_WorldIndicatorLayer.h"
#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "UI/WorldIndicator/CC_WorldIndicatorWidget.h"
#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/AssetManager.h"
#include "GAS_Demo.h"

namespace
{
	bool ImplementsIndicatorWidget(const UUserWidget* Widget)
	{
		return Widget && Widget->GetClass()->ImplementsInterface(UCC_WorldIndicatorWidget::StaticClass());
	}
}

UCC_WorldIndicatorLayer::UCC_WorldIndicatorLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WidgetPool(*this)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCC_WorldIndicatorLayer::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IndicatorCanvas"));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;
}

// 每次 Slate 构建（含切图后重新挂载）重新订阅，并补上已经存在的指示器。
void UCC_WorldIndicatorLayer::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime()) return;
	UCC_WorldIndicatorSubsystem* Subsystem = UCC_WorldIndicatorSubsystem::GetForPlayer(GetOwningPlayer());
	if (!Subsystem) return;
	BoundSubsystem = Subsystem;
	AddedHandle = Subsystem->OnIndicatorAdded.AddUObject(this, &ThisClass::HandleIndicatorAdded);
	RemovedHandle = Subsystem->OnIndicatorRemoved.AddUObject(this, &ThisClass::HandleIndicatorRemoved);
	for (UCC_WorldIndicator* Indicator : Subsystem->GetIndicators()) ShowIndicator(Indicator);
}

void UCC_WorldIndicatorLayer::NativeDestruct()
{
	if (UCC_WorldIndicatorSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnIndicatorAdded.Remove(AddedHandle);
		Subsystem->OnIndicatorRemoved.Remove(RemovedHandle);
	}
	BoundSubsystem.Reset();
	ReleaseAll();
	Super::NativeDestruct();
}

void UCC_WorldIndicatorLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	WidgetPool.ReleaseAllSlateResources();
}

void UCC_WorldIndicatorLayer::HandleIndicatorAdded(UCC_WorldIndicator* Indicator) { ShowIndicator(Indicator); }
void UCC_WorldIndicatorLayer::HandleIndicatorRemoved(UCC_WorldIndicator* Indicator) { HideIndicator(Indicator); }

void UCC_WorldIndicatorLayer::ShowIndicator(UCC_WorldIndicator* Indicator)
{
	if (!IsValid(Indicator) || ActiveWidgets.Contains(Indicator) || PendingLoads.Contains(Indicator)) return;
	const TSoftClassPtr<UUserWidget>& SoftClass = Indicator->WidgetClass;
	if (SoftClass.IsNull()) return;
	if (UClass* Loaded = SoftClass.Get())
	{
		CreateWidgetFor(Indicator, Loaded);
		return;
	}
	// 首次用到的控件类异步加载，避免卡顿；加载完成时指示器可能已被移除。
	// 先占位再发请求：已在内存中的资源可能在 RequestAsyncLoad 内部就同步回调。
	const TWeakObjectPtr<UCC_WorldIndicator> WeakIndicator = Indicator;
	PendingLoads.Add(Indicator);
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, WeakIndicator]()
		{
			if (PendingLoads.Remove(WeakIndicator) == 0) return; // 加载期间已被移除。
			UCC_WorldIndicator* Ready = WeakIndicator.Get();
			const UCC_WorldIndicatorSubsystem* Subsystem = BoundSubsystem.Get();
			if (Ready && Subsystem && Subsystem->GetIndicators().Contains(Ready)) CreateWidgetFor(Ready, Ready->WidgetClass.Get());
		}));
	if (TSharedPtr<FStreamableHandle>* Pending = PendingLoads.Find(Indicator)) *Pending = Handle;
}

void UCC_WorldIndicatorLayer::CreateWidgetFor(UCC_WorldIndicator* Indicator, UClass* WidgetClass)
{
	if (!Canvas || !WidgetClass || !WidgetClass->IsChildOf<UUserWidget>() || WidgetClass->HasAnyClassFlags(CLASS_Abstract)) return;
	UUserWidget* Widget = WidgetPool.GetOrCreateInstance(TSubclassOf<UUserWidget>(WidgetClass));
	if (!Widget) return;
	if (!ImplementsIndicatorWidget(Widget))
		UE_LOG(LogGAS_Demo, Warning, TEXT("世界指示器控件 %s 没有实现 CC World Indicator Widget 接口，只显示、不绑定数据。"), *GetNameSafe(WidgetClass));

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot || Widget->GetParent() != Canvas.Get()) CanvasSlot = Canvas->AddChildToCanvas(Widget);
	if (CanvasSlot)
	{
		// Slot 只在这里设置一次；之后每帧只改渲染平移。
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
		CanvasSlot->SetAlignment(Indicator->Alignment);
		CanvasSlot->SetZOrder(Indicator->Priority);
	}
	Widget->SetVisibility(ESlateVisibility::Hidden); // 第一帧投影前不显示在左上角。
	FCC_WorldIndicatorWidgetEntry& Entry = ActiveWidgets.Add(Indicator);
	Entry.Widget = Widget;
	Entry.bShown = false;
	if (ImplementsIndicatorWidget(Widget)) ICC_WorldIndicatorWidget::Execute_BindIndicator(Widget, Indicator);
}

void UCC_WorldIndicatorLayer::HideIndicator(UCC_WorldIndicator* Indicator)
{
	PendingLoads.Remove(Indicator);
	FCC_WorldIndicatorWidgetEntry Entry;
	if (!ActiveWidgets.RemoveAndCopyValue(Indicator, Entry) || !Entry.Widget) return;
	if (ImplementsIndicatorWidget(Entry.Widget)) ICC_WorldIndicatorWidget::Execute_UnbindIndicator(Entry.Widget);
	// 留在画布里折叠，下次复用时不必重新创建 Slot。
	Entry.Widget->SetVisibility(ESlateVisibility::Collapsed);
	WidgetPool.Release(Entry.Widget, false);
}

void UCC_WorldIndicatorLayer::ReleaseAll()
{
	for (auto& Pair : PendingLoads) if (Pair.Value.IsValid()) Pair.Value->CancelHandle();
	PendingLoads.Reset();
	TArray<TObjectPtr<UCC_WorldIndicator>> Indicators;
	ActiveWidgets.GetKeys(Indicators);
	for (UCC_WorldIndicator* Indicator : Indicators) HideIndicator(Indicator);
}

void UCC_WorldIndicatorLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ActiveWidgets.IsEmpty()) return;
	FCC_WorldOverlayProjector Projector;
	const bool bCanProject = Projector.Initialize(GetOwningPlayer(), MyGeometry);
	for (auto& Pair : ActiveWidgets)
	{
		const UCC_WorldIndicator* Indicator = Pair.Key;
		FCC_WorldIndicatorWidgetEntry& Entry = Pair.Value;
		if (!Entry.Widget) continue;
		FVector Location;
		FVector2D PlayerLocal;
		bool bShow = bCanProject && Indicator->bVisible && Indicator->GetAnchorLocation(Location)
			&& (Indicator->MaxDistance <= 0.f || FVector::DistSquared(Projector.GetViewOrigin(), Location) <= FMath::Square(Indicator->MaxDistance))
			&& Projector.ProjectToPlayerLocal(Location, PlayerLocal, true);
		if (bShow) Entry.Widget->SetRenderTranslation(Projector.PlayerLocalToWidget(PlayerLocal) + Indicator->ScreenOffset);
		if (bShow != Entry.bShown)
		{
			Entry.Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			Entry.bShown = bShow;
		}
	}
}
