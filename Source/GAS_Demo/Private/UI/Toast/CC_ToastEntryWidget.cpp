#include "UI/Toast/CC_ToastEntryWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size, bool bBold)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		if (bBold) Font.TypefaceFontName = TEXT("Bold");
		Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		return Text;
	}
}

UCC_ToastEntryWidget::UCC_ToastEntryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	StyleColors.Add(ECC_ToastStyle::Info, FLinearColor(0.30f, 0.62f, 1.00f));
	StyleColors.Add(ECC_ToastStyle::Success, FLinearColor(1.00f, 0.78f, 0.25f));
	StyleColors.Add(ECC_ToastStyle::Warning, FLinearColor(1.00f, 0.52f, 0.16f));
	StyleColors.Add(ECC_ToastStyle::Danger, FLinearColor(0.95f, 0.24f, 0.24f));
}

void UCC_ToastEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildNativeLayout();
	}
}

void UCC_ToastEntryWidget::BuildNativeLayout()
{
	// [色条 | 标题/正文] 的深色卡片，宽 360。
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(360.f);
	WidgetTree->RootWidget = Size;

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.82f));
	Card->SetPadding(FMargin(0.f));
	Size->AddChild(Card);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Card->AddChild(Row);

	Border_Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Accent"));
	UHorizontalBoxSlot* AccentSlot = Row->AddChildToHorizontalBox(Border_Accent);
	AccentSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* AccentWidth = WidgetTree->ConstructWidget<USizeBox>();
	AccentWidth->SetWidthOverride(5.f);
	Border_Accent->AddChild(AccentWidth);

	Image_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Icon"));
	Image_Icon->SetDesiredSizeOverride(FVector2D(36.f, 36.f));
	UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(Image_Icon);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Texts);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(14.f, 10.f, 14.f, 11.f));

	TextBlock_Title = MakeText(WidgetTree, TEXT("TextBlock_Title"), 17, true);
	Texts->AddChildToVerticalBox(TextBlock_Title);
	TextBlock_Message = MakeText(WidgetTree, TEXT("TextBlock_Message"), 13, false);
	TextBlock_Message->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.82f, 0.88f)));
	TextBlock_Message->SetAutoWrapText(true);
	Texts->AddChildToVerticalBox(TextBlock_Message)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
}

FLinearColor UCC_ToastEntryWidget::GetStyleColor(ECC_ToastStyle Style) const
{
	const FLinearColor* Color = StyleColors.Find(Style);
	return Color ? *Color : FLinearColor::White;
}

float UCC_ToastEntryWidget::GetAnimLength(const UWidgetAnimation* Animation)
{
	return Animation ? FMath::Max(0.f, Animation->GetEndTime() - Animation->GetStartTime()) : 0.f;
}

void UCC_ToastEntryWidget::ApplyRequest(const FCC_ToastRequest& Request)
{
	const bool bMerged = bHasContent;
	CurrentRequest = Request;
	bHasContent = true;
	const FLinearColor Accent = GetStyleColor(Request.Style);

	if (TextBlock_Title)
	{
		TextBlock_Title->SetText(Request.Title);
		TextBlock_Title->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TextBlock_Message)
	{
		TextBlock_Message->SetText(Request.Message);
		TextBlock_Message->SetVisibility(Request.Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Image_Icon)
	{
		if (Request.Icon)
		{
			Image_Icon->SetBrushFromTexture(Request.Icon);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Border_Accent)
	{
		Border_Accent->SetBrushColor(Accent);
	}
	if (bMerged && Anim_Refresh)
	{
		PlayAnimation(Anim_Refresh);
	}
	BP_OnToastApplied(Request, Accent, bMerged);
}

void UCC_ToastEntryWidget::NotifyPhase(ECC_ToastPhase Phase)
{
	if (!bPhaseNotified || Phase != LastPhase)
	{
		bPhaseNotified = true;
		LastPhase = Phase;
		BP_OnToastPhaseChanged(Phase);
	}
}

float UCC_ToastEntryWidget::PlayIntro()
{
	NotifyPhase(ECC_ToastPhase::Entering);
	if (Anim_In)
	{
		PlayAnimation(Anim_In);
		return GetAnimLength(Anim_In);
	}
	UpdateFallback(ECC_ToastPhase::Entering, 0.f);
	return FallbackIntroTime;
}

float UCC_ToastEntryWidget::PlayOutro()
{
	NotifyPhase(ECC_ToastPhase::Leaving);
	if (Anim_In && IsAnimationPlaying(Anim_In))
	{
		StopAnimation(Anim_In);
	}
	if (Anim_Out)
	{
		PlayAnimation(Anim_Out);
		return GetAnimLength(Anim_Out);
	}
	return FallbackOutroTime;
}

void UCC_ToastEntryWidget::UpdateFallback(ECC_ToastPhase Phase, float Alpha)
{
	if (Phase == ECC_ToastPhase::Showing)
	{
		NotifyPhase(ECC_ToastPhase::Showing);
	}
	// 有对应 UMG 动画时由动画负责表现。
	if ((Phase == ECC_ToastPhase::Entering && Anim_In) || (Phase == ECC_ToastPhase::Leaving && Anim_Out)) return;
	if (Phase == ECC_ToastPhase::Showing && Anim_In) return;

	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	float Opacity = 1.f;
	float OffsetY = 0.f;
	if (Phase == ECC_ToastPhase::Entering)
	{
		Opacity = Alpha;
		OffsetY = FallbackRiseDistance * (1.f - Eased);   // 从下方升上来
	}
	else if (Phase == ECC_ToastPhase::Leaving)
	{
		Opacity = 1.f - Alpha;
		OffsetY = -FallbackRiseDistance * Eased;          // 继续往上飘走
	}
	SetRenderOpacity(Opacity);
	SetRenderTranslation(FVector2D(0.f, OffsetY));
}
