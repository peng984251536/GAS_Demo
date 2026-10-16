#include "UI/BuffSelection/CC_BuffOfferCardWidget.h"
#include "UI/BuffSelection/CC_BuffSelectionScreen.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UCC_BuffOfferCardWidget::SetCardData(const FCC_BuffOfferCardData& InData, UCC_BuffSelectionScreen* InOwner)
{
    Data = InData;
    OwnerScreen = InOwner;
    if (Text_Name)
    {
        Text_Name->SetText(Data.DisplayName);
    }
    if (Text_Description)
    {
        Text_Description->SetText(Data.Description);
    }
    if (Image_Icon)
    {
        Image_Icon->SetBrushFromTexture(Data.Icon);
        Image_Icon->SetVisibility(Data.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    OnCardDataSet(Data);
}

void UCC_BuffOfferCardWidget::NativeOnClicked()
{
    Super::NativeOnClicked();
    if (OwnerScreen.IsValid())
    {
        OwnerScreen->ChooseOption(Data.OptionIndex);
    }
}
