#include "UI/BuffSelection/CC_BuffSelectionController.h"
#include "Components/CC_BuffSelectionComponent.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"

FCC_BuffOfferCardData FCC_BuffOfferCardData::FromOption(const FCC_BuffOfferOption& Option, int32 Index)
{
    FCC_BuffOfferCardData Card;
    Card.OptionIndex = Index;
    Card.Effect = Option.Effect;
    Card.Level = Option.Level;
    const UGameplayEffect* Definition = Option.Effect ? Option.Effect.GetDefaultObject() : nullptr;
    if (const UCC_BuffUIData* UIData = Definition ? Definition->FindComponent<UCC_BuffUIData>() : nullptr)
    {
        Card.DisplayName = UIData->DisplayName;
        Card.Description = UIData->Description;
        Card.Icon = UIData->Icon;
    }
    if (Card.DisplayName.IsEmpty())
    {
        Card.DisplayName = FText::FromString(GetNameSafe(Option.Effect.Get()));
    }
    return Card;
}

void UCC_BuffSelectionModel::SetState(int32 InOfferId, TArray<FCC_BuffOfferCardData>&& InCards, bool bInSubmitting,
                                      ECC_BuffChoiceResult InLastResult)
{
    if (OfferId == InOfferId && Cards == InCards && bSubmitting == bInSubmitting && LastResult == InLastResult)
    {
        return;
    }
    OfferId = InOfferId;
    Cards = MoveTemp(InCards);
    bSubmitting = bInSubmitting;
    LastResult = InLastResult;
    NotifyChanged();
}

UCC_BuffSelectionController::UCC_BuffSelectionController()
{
    ModelClass = UCC_BuffSelectionModel::StaticClass();
}

void UCC_BuffSelectionController::OnActivated()
{
    Selection = UCC_BuffSelectionComponent::FindForPlayer(GetPlayerController());
    if (Selection.IsValid())
    {
        Selection->OnOfferChanged.AddUniqueDynamic(this, &ThisClass::HandleOfferChanged);
        Selection->OnChoiceResolved.AddUniqueDynamic(this, &ThisClass::HandleChoiceResolved);
        Selection->ServerSetSelectionActive(true);
    }
    Refresh();
    Super::OnActivated();
}

void UCC_BuffSelectionController::OnDeactivated()
{
    if (Selection.IsValid())
    {
        Selection->OnOfferChanged.RemoveDynamic(this, &ThisClass::HandleOfferChanged);
        Selection->OnChoiceResolved.RemoveDynamic(this, &ThisClass::HandleChoiceResolved);
        Selection->ServerSetSelectionActive(false);
    }
    Selection.Reset();
    Super::OnDeactivated();
}

bool UCC_BuffSelectionController::ChooseOption(int32 OptionIndex)
{
    const UCC_BuffSelectionModel* SelectionModel = Cast<UCC_BuffSelectionModel>(GetModel());
    if (!CanHandleActions() || !Selection.IsValid() || !SelectionModel || !SelectionModel->HasOffer())
    {
        return false;
    }
    const bool bSent = Selection->RequestChoose(SelectionModel->GetOfferId(), OptionIndex);
    Refresh();
    return bSent;
}

void UCC_BuffSelectionController::HandleOfferChanged() { Refresh(); }

void UCC_BuffSelectionController::HandleChoiceResolved(int32 OfferId, ECC_BuffChoiceResult Result)
{
    if (Result != ECC_BuffChoiceResult::Accepted)
    {
        LastResult = Result;
        LastResultOfferId = OfferId;
    }
    Refresh();
}

void UCC_BuffSelectionController::Refresh()
{
    UCC_BuffSelectionModel* SelectionModel = Cast<UCC_BuffSelectionModel>(GetModel());
    if (!IsActive() || !SelectionModel)
    {
        return;
    }
    const FCC_BuffOffer Offer = Selection.IsValid() ? Selection->GetCurrentOffer() : FCC_BuffOffer();
    TArray<FCC_BuffOfferCardData> Cards;
    if (Offer.IsValid())
    {
        for (int32 Index = 0; Index < Offer.Options.Num(); ++Index)
        {
            Cards.Add(FCC_BuffOfferCardData::FromOption(Offer.Options[Index], Index));
        }
    }
    // 失败提示只属于产生它的那一轮。
    const ECC_BuffChoiceResult Result =
        LastResultOfferId == Offer.OfferId ? LastResult : ECC_BuffChoiceResult::None;
    SelectionModel->SetState(Offer.IsValid() ? Offer.OfferId : 0, MoveTemp(Cards),
                             Selection.IsValid() && Selection->IsChoiceInFlight(), Result);
}
