#include "UI/HealthBar/CC_HealthBarWidget.h"
#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"

UCC_HealthBarWidget::UCC_HealthBarWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 没有设计树时搭默认外观：Border(边框) → SizeBox(尺寸) → Overlay[背景, 左对齐 SizeBox → 填充]。
void UCC_HealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree->RootWidget) return;
	Border_Frame = WidgetTree->ConstructWidget<UBorder>();
	WidgetTree->RootWidget = Border_Frame;
	SizeBox_Bar = WidgetTree->ConstructWidget<USizeBox>();
	Border_Frame->SetContent(SizeBox_Bar);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox_Bar->SetContent(Layers);
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetColorAndOpacity(BackgroundColor);
	if (UOverlaySlot* BackgroundSlot = Layers->AddChildToOverlay(Background))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}
	SizeBox_Fill = WidgetTree->ConstructWidget<USizeBox>();
	Image_Fill = WidgetTree->ConstructWidget<UImage>();
	Image_Fill->SetColorAndOpacity(FillColor);
	SizeBox_Fill->SetContent(Image_Fill);
	if (UOverlaySlot* FillSlot = Layers->AddChildToOverlay(SizeBox_Fill))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
	}
	const float Border = FMath::Max(0.f, BorderWidth);
	Border_Frame->SetBrushColor(Border > 0.f ? BorderColor : FLinearColor::Transparent);
	Border_Frame->SetPadding(FMargin(Border));
	SizeBox_Bar->SetWidthOverride(BarSize.X);
	SizeBox_Bar->SetHeightOverride(BarSize.Y);
}

void UCC_HealthBarWidget::BindIndicator_Implementation(UCC_WorldIndicator* Indicator)
{
	UnbindIndicator_Implementation(); // 对象池复用：先清掉上一个角色的订阅。
	AActor* Actor = Indicator ? Indicator->GetAnchorActor() : nullptr;
	TargetActor = Actor;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (ASC && ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute()))
	{
		BoundASC = ASC;
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleAttributeChanged);
		MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &ThisClass::HandleAttributeChanged);
	}
	ReadVitals();
	DisplayedFraction = TargetFraction(); // 首次显示直接定位，不从空血条开始。
	OnHealthBarBound(Actor);
	RefreshView();
}

void UCC_HealthBarWidget::UnbindIndicator_Implementation()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	}
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	BoundASC.Reset();
	TargetActor.Reset();
	Health = MaxHealth = DisplayedFraction = 0.f;
}

void UCC_HealthBarWidget::ReadVitals()
{
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	Health = ASC ? ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()) : 0.f;
	MaxHealth = ASC ? ASC->GetNumericAttribute(UCC_AttributeSet::GetMaxHealthAttribute()) : 0.f;
}

void UCC_HealthBarWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	ReadVitals();
	if (InterpSpeed <= 0.f) DisplayedFraction = TargetFraction();
	RefreshView();
}

// 只在追赶目标比例时有实际工作；比例一致后每帧只做一次比较。
void UCC_HealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Target = TargetFraction();
	if (FMath::IsNearlyEqual(DisplayedFraction, Target, 0.001f)) return;
	DisplayedFraction = FMath::FInterpTo(DisplayedFraction, Target, InDeltaTime, InterpSpeed);
	if (FMath::IsNearlyEqual(DisplayedFraction, Target, 0.001f)) DisplayedFraction = Target;
	RefreshView();
}

void UCC_HealthBarWidget::RefreshView()
{
	const bool bHidden = MaxHealth <= 0.f || (bHideWhenDead && Health <= 0.f) || (bHideWhenFull && Health >= MaxHealth);
	if (UWidget* Root = WidgetTree ? WidgetTree->RootWidget : nullptr)
		Root->SetVisibility(bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (SizeBox_Fill && Image_Fill)
	{
		SizeBox_Fill->SetWidthOverride(BarSize.X * DisplayedFraction);
		Image_Fill->SetVisibility(DisplayedFraction > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	OnHealthBarUpdated(Health, MaxHealth, DisplayedFraction);
}
