#include "UI/Character/CC_CharacterBuffEntryWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UCC_CharacterBuffEntryWidget::SetBuff(const FCC_CharacterBuffEntry& InBuff)
{
	Buff = InBuff;
	if (Icon)
	{
		Icon->SetVisibility(Buff.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Buff.Icon) Icon->SetBrushFromTexture(Buff.Icon);
	}
	if (NameText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Buff.DisplayName);
		Args.Add(TEXT("Stacks"), Buff.StackCount);
		NameText->SetText(Buff.StackCount > 1 ? FText::Format(NameWithStacksFormat, Args) : Buff.DisplayName);
	}
	if (DescriptionText) DescriptionText->SetText(Buff.Description);
	UpdateTime();
	OnBuffSet(Buff);
}

float UCC_CharacterBuffEntryWidget::GetRemainingTime() const
{
	return CCCharacterScreen::GetRemainingTime(GetWorld(), Buff);
}

void UCC_CharacterBuffEntryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Buff.IsInfinite()) UpdateTime();
}

// 10 秒以上显示整数秒，以下保留一位小数；到 0 后等 BuffComponent 移除条目再消失。
void UCC_CharacterBuffEntryWidget::UpdateTime()
{
	const float Remaining = GetRemainingTime();
	if (TimeText)
	{
		if (Remaining < 0.f)
		{
			TimeText->SetText(InfiniteDurationText);
		}
		else
		{
			const int32 Digits = Remaining >= 10.f ? 0 : 1;
			FNumberFormattingOptions Format;
			Format.SetMinimumFractionalDigits(Digits).SetMaximumFractionalDigits(Digits);
			TimeText->SetText(FText::Format(NSLOCTEXT("CCCharacterScreen", "RemainingSeconds", "{0}s"), FText::AsNumber(Remaining, &Format)));
		}
	}
	if (TimeBar)
	{
		TimeBar->SetPercent(Remaining < 0.f ? 1.f : (Buff.Duration > 0.f ? FMath::Clamp(Remaining / Buff.Duration, 0.f, 1.f) : 0.f));
	}
}
