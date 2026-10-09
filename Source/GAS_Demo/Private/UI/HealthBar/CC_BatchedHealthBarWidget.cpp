#include "UI/HealthBar/CC_BatchedHealthBarWidget.h"
#include "UI/HealthBar/CC_HealthBarItem.h"
#include "UI/HealthBar/CC_HealthBarOverlayController.h"
#include "UI/HealthBar/CC_DefaultHealthBarItemWidget.h"
#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GAS_Demo.h"

UCC_BatchedHealthBarWidget::UCC_BatchedHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 全屏层只负责摆放，不阻挡鼠标点击或 HUD 按钮。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ControllerClass = UCC_HealthBarOverlayController::StaticClass();
	DefaultItemWidgetClass = UCC_DefaultHealthBarItemWidget::StaticClass();
}

void UCC_BatchedHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ItemCanvas) return; // 蓝图里放了名为 ItemCanvas 的画布。
	if (!WidgetTree->RootWidget)
	{
		ItemCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ItemCanvas"));
		WidgetTree->RootWidget = ItemCanvas;
	}
	else
	{
		ItemCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	}
	if (ItemCanvas) ItemCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	else UE_LOG(LogGAS_Demo, Warning, TEXT("%s：找不到放血条的画布。请把根控件设为 Canvas Panel，或添加一个名为 ItemCanvas 的 Canvas Panel。"), *GetPathName());
}

// 每次 Slate 构建开始一次控制器会话；切图重新挂载时会得到新的会话。
void UCC_BatchedHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime() || OverlayController) return;
	OverlayController = UCC_UIController::CreateForView(this, ControllerClass);
	if (OverlayController) OverlayController->Activate();
}

void UCC_BatchedHealthBarWidget::NativeDestruct()
{
	ReleaseAllItems();
	if (OverlayController) OverlayController->Release();
	OverlayController = nullptr;
	Super::NativeDestruct();
}

UUserWidget* UCC_BatchedHealthBarWidget::AcquireItem(UClass* ItemClass)
{
	if (TArray<TWeakObjectPtr<UUserWidget>>* Pool = FreeItems.Find(ItemClass))
	{
		while (!Pool->IsEmpty())
		{
			if (UUserWidget* Pooled = Pool->Pop(EAllowShrinking::No).Get())
			{
				Pooled->SetVisibility(ESlateVisibility::HitTestInvisible);
				return Pooled;
			}
		}
	}
	UUserWidget* Widget = CreateWidget<UUserWidget>(this, ItemClass);
	if (!Widget) return nullptr;
	if (!ItemClass->ImplementsInterface(UCC_HealthBarItem::StaticClass()))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("头顶血条控件 %s 没有实现 CC Health Bar Item 接口，只会显示、不会收到血量更新。"), *GetNameSafe(ItemClass));
	}
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = ItemCanvas->AddChildToCanvas(Widget))
	{
		// 按控件自身期望尺寸显示，底边中点对齐头顶锚点。
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}
	return Widget;
}

void UCC_BatchedHealthBarWidget::ReleaseItem(UUserWidget* Widget)
{
	if (!Widget) return;
	if (Widget->GetClass()->ImplementsInterface(UCC_HealthBarItem::StaticClass())) ICC_HealthBarItem::Execute_OnHealthBarReleased(Widget);
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	FreeItems.FindOrAdd(Widget->GetClass()).Add(Widget);
}

void UCC_BatchedHealthBarWidget::ReleaseAllItems()
{
	for (auto& Pair : ActiveItems) ReleaseItem(Pair.Value.Widget.Get());
	ActiveItems.Reset();
}

void UCC_BatchedHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UCC_HealthBarOverlayModel* Model = OverlayController ? Cast<UCC_HealthBarOverlayModel>(OverlayController->GetModel()) : nullptr;
	FCC_WorldOverlayProjector Projector;
	if (!ItemCanvas || !Model || Model->GetItems().IsEmpty() || !Projector.Initialize(GetOwningPlayer(), MyGeometry))
	{
		ReleaseAllItems();
		return;
	}
	for (auto& Pair : ActiveItems) Pair.Value.bSeenThisFrame = false;

	for (const FCC_HealthBarDisplayItem& Item : Model->GetItems())
	{
		AActor* Actor = Item.Actor.Get();
		if (!Actor) continue;
		if (Item.MaxDistance > 0 && FVector::DistSquared(Projector.GetViewOrigin(), Item.WorldAnchor) > FMath::Square(Item.MaxDistance)) continue;
		// 锚点在背后或画面外时不显示；不做场景遮挡检测，默认允许透墙显示。
		FVector2D PlayerLocal;
		if (!Projector.ProjectToPlayerLocal(Item.WorldAnchor, PlayerLocal, true)) continue;
		UClass* ItemClass = Item.ItemWidgetClass ? Item.ItemWidgetClass.Get() : DefaultItemWidgetClass.Get();
		if (!ItemClass || ItemClass->HasAnyClassFlags(CLASS_Abstract)) continue;

		FActiveItem* Active = ActiveItems.Find(Actor);
		// 配置换了控件类（例如重新注册），旧控件回池，按新类重新分配。
		if (Active && (!Active->Widget.IsValid() || Active->Widget->GetClass() != ItemClass))
		{
			ReleaseItem(Active->Widget.Get());
			ActiveItems.Remove(Actor);
			Active = nullptr;
		}
		if (!Active)
		{
			UUserWidget* Widget = AcquireItem(ItemClass);
			if (!Widget) continue;
			Active = &ActiveItems.Add(Actor);
			Active->Widget = Widget;
			if (ItemClass->ImplementsInterface(UCC_HealthBarItem::StaticClass())) ICC_HealthBarItem::Execute_OnHealthBarAssigned(Widget, Actor);
		}
		Active->bSeenThisFrame = true;
		UUserWidget* Widget = Active->Widget.Get();
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			CanvasSlot->SetPosition(Projector.PlayerLocalToWidget(PlayerLocal) + ScreenOffset);
		if ((!Active->bHasData || Active->LastData != Item.Data) && ItemClass->ImplementsInterface(UCC_HealthBarItem::StaticClass()))
		{
			ICC_HealthBarItem::Execute_OnHealthBarUpdated(Widget, Item.Data);
			Active->LastData = Item.Data;
			Active->bHasData = true;
		}
	}

	// 本帧没出现的（移除、隐藏、出画面、超距离）回池。
	for (auto It = ActiveItems.CreateIterator(); It; ++It)
	{
		if (It.Value().bSeenThisFrame) continue;
		ReleaseItem(It.Value().Widget.Get());
		It.RemoveCurrent();
	}
}
