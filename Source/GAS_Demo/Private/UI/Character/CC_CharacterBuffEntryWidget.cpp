#include "UI/Character/CC_CharacterBuffEntryWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Utils/CC_BlueprintLibrary.h"

void UCC_CharacterBuffEntryWidget::SetBuff(const FCC_CharacterBuffEntry& InBuff)
{
	Buff = InBuff;
	if (Icon)
	{
		//Icon->SetVisibility(Buff.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Buff.Icon)
		{
			Icon->SetBrushFromTexture(Buff.Icon);
		}
		else
		{
			const FText Namet = Buff.DisplayName;
			const FString& NameString = Namet.ToString();
			FLinearColor c = UCC_BlueprintLibrary::GetColorFromString(NameString);
			Icon->SetColorAndOpacity(c);
		}

		// if (NameText)
		// {
		// 	FFormatNamedArguments Args;
		// 	Args.Add(TEXT("Name"), Buff.DisplayName);
		// 	Args.Add(TEXT("Stacks"), Buff.StackCount);
		// 	NameText->SetText(Buff.StackCount > 1 ? FText::Format(NameWithStacksFormat, Args) : Buff.DisplayName);
		// }
		if (DescriptionText)
		{
			const FString DescriptionT =
				Buff.DisplayName.ToString() + TEXT(":") + Buff.Description.ToString();

			DescriptionText->SetText(FText::FromString(DescriptionT));
		}
	}

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

	const float Now = CCCharacterScreen::GetServerTime(GetWorld());
	UE_LOG(LogTemp, Warning,
		TEXT("End=%.3f Now=%.3f Remaining=%.3f"),
		Buff.EndServerTime, Now, Remaining);

	
	// 后面保留 TimeText 和 TimeBar 的更新逻辑
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
			TimeText->SetText(FText::Format(
				NSLOCTEXT("CCCharacterScreen", "RemainingSeconds", "{0}s"), FText::AsNumber(Remaining, &Format)));
		}
	}
	if (TimeBar)
	{
		TimeBar->SetPercent(Remaining < 0.f
			                    ? 1.f
			                    : (Buff.Duration > 0.f ? FMath::Clamp(Remaining / Buff.Duration, 0.f, 1.f) : 0.f));
	}
}
