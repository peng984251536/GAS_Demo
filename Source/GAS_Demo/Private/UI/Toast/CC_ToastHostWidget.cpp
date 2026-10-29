#include "UI/Toast/CC_ToastHostWidget.h"
#include "UI/Toast/CC_ToastEntryWidget.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Audio/CC_AudioLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UCC_ToastHostWidget::UCC_ToastHostWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	EntryClass = UCC_ToastEntryWidget::StaticClass();
}

void UCC_ToastHostWidget::SetEntryClassOverride(TSubclassOf<UCC_ToastEntryWidget> InClass)
{
	if (InClass) EntryClass = InClass;
}

void UCC_ToastHostWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildNativeLayout();
	}
}

void UCC_ToastHostWidget::BuildNativeLayout()
{
	// 屏幕下方居中，往上堆叠。
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ToastRoot"));
	WidgetTree->RootWidget = Root;
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel_Stack"));
	UOverlaySlot* StackSlot = Root->AddChildToOverlay(Stack);
	StackSlot->SetHorizontalAlignment(HAlign_Center);
	StackSlot->SetVerticalAlignment(VAlign_Bottom);
	StackSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 220.f));
	Panel_Stack = Stack;
}

FCC_ActiveToast* UCC_ToastHostWidget::FindActive(FName DedupeKey)
{
	if (DedupeKey.IsNone()) return nullptr;
	return Active.FindByPredicate([DedupeKey](const FCC_ActiveToast& Toast)
	{
		return Toast.DedupeKey == DedupeKey && Toast.Phase != ECC_ToastPhase::Leaving && Toast.Widget;
	});
}

int32 UCC_ToastHostWidget::CountActive(bool bIncludeLeaving) const
{
	int32 Count = 0;
	for (const FCC_ActiveToast& Toast : Active)
	{
		if (bIncludeLeaving || Toast.Phase != ECC_ToastPhase::Leaving) ++Count;
	}
	return Count;
}

void UCC_ToastHostWidget::Enqueue(const FCC_ToastRequest& Request)
{
	if (Request.Title.IsEmpty() && Request.Message.IsEmpty()) return;
	if (FCC_ActiveToast* Existing = FindActive(Request.DedupeKey))
	{
		Existing->Widget->ApplyRequest(Request);
		Existing->HoldTime = Request.Duration > 0.f ? Request.Duration : DefaultDuration;
		if (Existing->Phase == ECC_ToastPhase::Showing)
		{
			Existing->PhaseTime = 0.f;
			Existing->PhaseLength = Existing->HoldTime;
		}
		return;
	}
	Queue.Push(Request, MaxQueued);
}

void UCC_ToastHostWidget::StartToast(const FCC_ToastRequest& Request)
{
	if (!Panel_Stack || !EntryClass) return;
	UCC_ToastEntryWidget* Widget = CreateWidget<UCC_ToastEntryWidget>(this, EntryClass);
	if (!Widget) return;

	// 追加到最下面：新消息在底部，旧消息被往上推。
	UPanelSlot* AddedSlot = Panel_Stack->AddChild(Widget);
	if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(AddedSlot))
	{
		VSlot->SetHorizontalAlignment(HAlign_Center);
		VSlot->SetPadding(FMargin(0.f, EntrySpacing * 0.5f));
	}
	Widget->ApplyRequest(Request);

	FCC_ActiveToast& Toast = Active.AddDefaulted_GetRef();
	Toast.Widget = Widget;
	Toast.Phase = ECC_ToastPhase::Entering;
	Toast.PhaseLength = Widget->PlayIntro();
	Toast.HoldTime = Request.Duration > 0.f ? Request.Duration : DefaultDuration;
	Toast.DedupeKey = Request.DedupeKey;

	if (!Request.SoundEvent.IsNone())
	{
		UCC_AudioLibrary::PlayAudio2D(this, Request.SoundEvent);
	}
}

void UCC_ToastHostWidget::BeginLeaving(FCC_ActiveToast& Toast)
{
	if (Toast.Phase == ECC_ToastPhase::Leaving || !Toast.Widget) return;
	Toast.Phase = ECC_ToastPhase::Leaving;
	Toast.PhaseTime = 0.f;
	Toast.PhaseLength = Toast.Widget->PlayOutro();
}

bool UCC_ToastHostWidget::IsHeldByMenu() const
{
	if (!bHoldWhileMenuOpen) return false;
	const UCC_RootLayout* Root = UCC_UIManagerSubsystem::GetRootLayoutForPlayer(GetOwningPlayer());
	return Root && Root->HasMenu();
}

void UCC_ToastHostWidget::Advance(float DeltaTime)
{
	const bool bNowHeld = IsHeldByMenu();
	if (bNowHeld != bHeld)
	{
		bHeld = bNowHeld;
		SetRenderOpacity(bHeld ? 0.f : 1.f);
	}
	if (bHeld) return;

	// 1) 推进现有条目。
	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		FCC_ActiveToast& Toast = Active[Index];
		if (!Toast.Widget || !Toast.Widget->GetParent())
		{
			Active.RemoveAt(Index);
			continue;
		}
		Toast.PhaseTime += DeltaTime;
		const float Alpha = Toast.PhaseLength > 0.f ? Toast.PhaseTime / Toast.PhaseLength : 1.f;

		switch (Toast.Phase)
		{
		case ECC_ToastPhase::Entering:
			if (Alpha >= 1.f)
			{
				Toast.Phase = ECC_ToastPhase::Showing;
				Toast.PhaseTime = 0.f;
				Toast.PhaseLength = Toast.HoldTime;
				Toast.Widget->UpdateFallback(ECC_ToastPhase::Showing, 1.f);
			}
			else
			{
				Toast.Widget->UpdateFallback(ECC_ToastPhase::Entering, Alpha);
			}
			break;
		case ECC_ToastPhase::Showing:
			if (Alpha >= 1.f) BeginLeaving(Toast);
			break;
		case ECC_ToastPhase::Leaving:
			if (Alpha >= 1.f)
			{
				Toast.Widget->RemoveFromParent();
				Active.RemoveAt(Index);
				continue;
			}
			Toast.Widget->UpdateFallback(ECC_ToastPhase::Leaving, Alpha);
			break;
		}
	}

	// 2) 有空位就从队列补上（退场中的也占位，等它完全消失）。
	while (!Queue.IsEmpty() && CountActive(true) < MaxVisible)
	{
		FCC_ToastRequest Next;
		Queue.Pop(Next);
		StartToast(Next);
	}

	// 3) 满了还有排队：最早一条显示够 MinDisplayTimeWhenQueued 就提前退场。
	if (MinDisplayTimeWhenQueued > 0.f && !Queue.IsEmpty() && CountActive(false) >= MaxVisible)
	{
		for (FCC_ActiveToast& Toast : Active)
		{
			if (Toast.Phase == ECC_ToastPhase::Showing)
			{
				if (Toast.PhaseTime >= MinDisplayTimeWhenQueued) BeginLeaving(Toast);
				break;
			}
		}
	}
}

void UCC_ToastHostWidget::ClearAll()
{
	for (FCC_ActiveToast& Toast : Active)
	{
		if (Toast.Widget) Toast.Widget->RemoveFromParent();
	}
	Active.Reset();
	Queue.Reset();
}
