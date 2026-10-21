#include "UI/DamageText/CC_DamageNumberWidget.h"
#include "UI/DamageText/CC_DamageNumberSource.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

UCC_DamageNumberWidget::UCC_DamageNumberWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Font = FCoreStyle::GetDefaultFontStyle("Bold", 28);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCC_DamageNumberWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		TextBlock_Damage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextBlock_Damage"));
		TextBlock_Damage->SetJustification(ETextJustify::Center);
		WidgetTree->RootWidget = TextBlock_Damage;
	}
	if (UWidget* Root = WidgetTree->RootWidget) Root->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UCC_DamageNumberWidget::BindIndicator_Implementation(UCC_WorldIndicator* Indicator)
{
	BoundNumber = Cast<UCC_DamageNumberIndicator>(Indicator);
	Elapsed = 0.f;
	if (!BoundNumber) return;
	bCritical = BoundNumber->Style == ECC_DamageTextStyle::Critical;
	const bool bHeal = BoundNumber->Style == ECC_DamageTextStyle::Heal;

	if (TextBlock_Damage)
	{
		FNumberFormattingOptions Options;
		Options.SetUseGrouping(false);
		Options.SetMaximumFractionalDigits(0);
		const FText Number = FText::AsNumber(FMath::RoundToDouble(BoundNumber->Amount), &Options);
		TextBlock_Damage->SetText(bHeal ? FText::Format(NSLOCTEXT("DamageNumber", "Heal", "+{0}"), Number) : Number);
		FSlateFontInfo StyledFont = Font;
		StyledFont.Size = bCritical ? CriticalFontSize : FontSize;
		TextBlock_Damage->SetFont(StyledFont);
		TextBlock_Damage->SetColorAndOpacity(FSlateColor(bHeal ? HealingColor : (bCritical ? CriticalColor : DamageColor)));
	}
	if (bDriveAnimationInCode) ApplyCodeAnimation(0.f);
	OnDamageNumberShown(BoundNumber->Amount, BoundNumber->Style, BoundNumber->DamageType);
}

void UCC_DamageNumberWidget::UnbindIndicator_Implementation()
{
	BoundNumber = nullptr;
	StopAllAnimations();
	SetRenderOpacity(1.f);
	if (UWidget* Root = WidgetTree ? WidgetTree->RootWidget : nullptr) Root->SetRenderTransform(FWidgetTransform());
}

void UCC_DamageNumberWidget::FinishDamageNumber()
{
	if (!BoundNumber) return;
	UCC_DamageNumberIndicator* Number = BoundNumber;
	BoundNumber = nullptr; // 先清掉，防止同一帧重复结束。
	if (UCC_WorldIndicatorSubsystem* Indicators = UCC_WorldIndicatorSubsystem::GetForPlayer(GetOwningPlayer()))
		Indicators->RemoveIndicator(Number);
}

void UCC_DamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundNumber || !bDriveAnimationInCode) return;
	Elapsed += InDeltaTime;
	const float T = Elapsed / FMath::Max(0.1f, Lifetime);
	if (T >= 1.f)
	{
		FinishDamageNumber();
		return;
	}
	ApplyCodeAnimation(T);
}

// 前 15% 淡入、后 35% 淡出；上升按开方曲线先快后慢；暴击在出现时有一次缩放冲击。
void UCC_DamageNumberWidget::ApplyCodeAnimation(float T)
{
	const float Alpha = T < 0.15f ? T / 0.15f : FMath::Clamp((1.f - T) / 0.35f, 0.f, 1.f);
	SetRenderOpacity(Alpha);
	if (UWidget* Root = WidgetTree ? WidgetTree->RootWidget : nullptr)
	{
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(0.f, -RiseHeight * FMath::Sqrt(T));
		const float Punch = bCritical && T < 0.15f ? 1.f + 0.3f * (1.f - T / 0.15f) : 1.f;
		Transform.Scale = FVector2D(Punch, Punch);
		Root->SetRenderTransform(Transform);
	}
}
