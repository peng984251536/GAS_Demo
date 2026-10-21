#include "UI/BuffSelection/CC_BuffSelectionScreen.h"
#include "UI/BuffSelection/CC_BuffOfferCardWidget.h"
#include "UI/BuffSelection/CC_BuffSelectionController.h"
#include "Components/PanelWidget.h"

UCC_BuffSelectionScreen::UCC_BuffSelectionScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ControllerClass = UCC_BuffSelectionController::StaticClass();
    InputConfig = ECC_UIInputMode::Menu;
    // 必须选一张才能继续，返回键只被消费、不关闭页面。
    bAllowBack = false;
}

UCC_BuffSelectionModel* UCC_BuffSelectionScreen::GetSelectionModel() const
{
    return Cast<UCC_BuffSelectionModel>(GetUIModel());
}

bool UCC_BuffSelectionScreen::ChooseOption(int32 OptionIndex)
{
    UCC_BuffSelectionController* Controller = Cast<UCC_BuffSelectionController>(GetScreenController());
    return Controller && Controller->ChooseOption(OptionIndex);
}

void UCC_BuffSelectionScreen::NativeOnModelChanged()
{
    const UCC_BuffSelectionModel* Model = GetSelectionModel();
    if (!Model)
    {
        Super::NativeOnModelChanged();
        return;
    }
    RebuildCards(*Model);
    // 没有待选内容时关闭；在 Tick 里执行，避免在模型通知或激活过程中同步导航。
    bCloseRequested = !Model->HasOffer();
    OnBuffOfferChanged(Model->GetCards(), Model->IsSubmitting(), Model->GetLastResult());
    Super::NativeOnModelChanged();
}

void UCC_BuffSelectionScreen::RebuildCards(const UCC_BuffSelectionModel& Model)
{
    if (!PanelWidget_Cards || !CardClass)
    {
        return;
    }
    if (BuiltOfferId != Model.GetOfferId())
    {
        PanelWidget_Cards->ClearChildren();
        SpawnedCards.Reset();
        BuiltOfferId = Model.GetOfferId();
        for (const FCC_BuffOfferCardData& CardData : Model.GetCards())
        {
            UCC_BuffOfferCardWidget* Card = CreateWidget<UCC_BuffOfferCardWidget>(this, CardClass);
            if (!Card)
            {
                continue;
            }
            PanelWidget_Cards->AddChild(Card);
            Card->SetCardData(CardData, this);
            SpawnedCards.Add(Card);
        }
        // 新一轮的卡片生成后，让 CommonUI 把焦点交给第一张卡片。
        RefreshNavigationFocus();
    }
    // 提交中或本轮已结束（页面即将关闭、卡片仍在播放退场）时都不可点击。
    const bool bInteractable = Model.HasOffer() && !Model.IsSubmitting();
    for (UCC_BuffOfferCardWidget* Card : SpawnedCards)
    {
        if (Card)
        {
            Card->SetIsInteractionEnabled(bInteractable);
        }
    }
}

void UCC_BuffSelectionScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bCloseRequested && IsActivated() && CloseScreen())
    {
        bCloseRequested = false;
    }
}

void UCC_BuffSelectionScreen::NativeOnDeactivated()
{
    bCloseRequested = false;
    // 下次入栈（可能来自对象池）一定重建卡片。
    BuiltOfferId = 0;
    Super::NativeOnDeactivated();
}

UWidget* UCC_BuffSelectionScreen::NativeGetDesiredFocusTarget() const
{
    if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
    {
        return Target;
    }
    return SpawnedCards.IsEmpty() ? nullptr : SpawnedCards[0].Get();
}
