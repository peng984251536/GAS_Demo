#include "UI/Buff/CC_BuffBarWidget.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"

TSharedRef<SWidget> UCC_BuffBarWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        BuffBox = WidgetTree->ConstructWidget<UHorizontalBox>();
        WidgetTree->RootWidget = BuffBox;
    }
    return Super::RebuildWidget();
}

void UCC_BuffBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!Observed.IsValid()) SetObservedCharacter(Cast<ACC_BaseCharacter>(GetOwningPlayerPawn()));
    else { Observed->OnBuffsChanged.AddUniqueDynamic(this, &ThisClass::RefreshBuffs); RefreshBuffs(); }
}

void UCC_BuffBarWidget::SetObservedCharacter(ACC_BaseCharacter* Character)
{
    if (Observed.IsValid()) Observed->OnBuffsChanged.RemoveDynamic(this, &ThisClass::RefreshBuffs);
    Observed = Character ? Character->GetBuffComponent() : nullptr;
    if (Observed.IsValid()) Observed->OnBuffsChanged.AddUniqueDynamic(this, &ThisClass::RefreshBuffs);
    RefreshBuffs();
}

void UCC_BuffBarWidget::RefreshBuffs()
{
    if (!BuffBox) return;
    BuffBox->ClearChildren();
    Timers.Reset();
    InstanceIds.Reset();
    if (!Observed.IsValid()) return;
    for (const auto& Info : Observed->GetActiveBuffs())
    {
        const auto* Definition = Info.Definition ? Info.Definition.GetDefaultObject() : nullptr;
        if (!Definition) continue;
        auto* Cell = WidgetTree->ConstructWidget<UVerticalBox>();
        auto* CellSlot = BuffBox->AddChildToHorizontalBox(Cell);
        CellSlot->SetPadding(FMargin(6.f));
        Cell->SetToolTipText(Definition->Description);
        Cell->SetRenderOpacity(Info.bIsInhibited ? 0.4f : 1.f);
        if (Definition->Icon)
        {
            auto* Size = WidgetTree->ConstructWidget<USizeBox>();
            Size->SetWidthOverride(40.f);
            Size->SetHeightOverride(40.f);
            auto* Icon = WidgetTree->ConstructWidget<UImage>();
            Icon->SetBrushFromTexture(Definition->Icon);
            Size->AddChild(Icon);
            Cell->AddChildToVerticalBox(Size);
        }
        auto* Name = WidgetTree->ConstructWidget<UTextBlock>();
        Name->SetText(Info.StackCount > 1
            ? FText::Format(NSLOCTEXT("Buff", "NameWithStacks", "{0} ×{1}"), Definition->DisplayName, FText::AsNumber(Info.StackCount))
            : Definition->DisplayName);
        Cell->AddChildToVerticalBox(Name);
        auto* Timer = WidgetTree->ConstructWidget<UTextBlock>();
        Cell->AddChildToVerticalBox(Timer);
        Timers.Add(Timer);
        InstanceIds.Add(Info.InstanceId);
    }
}

void UCC_BuffBarWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!Observed.IsValid()) { if (!Timers.IsEmpty()) RefreshBuffs(); return; }
    for (int32 Index = 0; Index < Timers.Num(); ++Index)
    {
        const float Remaining = Observed->GetRemainingTime(InstanceIds[Index]);
        Timers[Index]->SetText(Remaining < 0.f ? FText::FromString(TEXT("∞")) : FText::AsNumber(FMath::CeilToInt(Remaining)));
    }
}

void UCC_BuffBarWidget::NativeDestruct()
{
    if (Observed.IsValid()) Observed->OnBuffsChanged.RemoveDynamic(this, &ThisClass::RefreshBuffs);
    Super::NativeDestruct();
}
