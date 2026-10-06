#include "UI/Framework/CC_DemoScreens.h"
#include "GameplayTags/CC_Tags.h"
#include "UI/Framework/CC_RootLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	// 创建示例全屏遮罩和居中面板；正式美术布局可以完全由蓝图替换。
	UVerticalBox* BuildPanel(UWidgetTree* Tree, const FText& Title, bool bDialog)
	{
		UOverlay* Root = Tree->ConstructWidget<UOverlay>();
		Tree->RootWidget = Root;
		UBorder* Shade = Tree->ConstructWidget<UBorder>();
		Shade->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.035f, bDialog ? 0.7f : 0.82f));
		UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(380.f);
		UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Size);
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Panel = Tree->ConstructWidget<UVerticalBox>();
		Size->AddChild(Panel);
		UTextBlock* Heading = Tree->ConstructWidget<UTextBlock>();
		Heading->SetText(Title);
		Heading->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Heading->GetFont();
		Font.Size = 30;
		Heading->SetFont(Font);
		Panel->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0, 0, 0, 24));
		return Panel;
	}
	// 创建带本地玩家上下文的 CommonButton，加入示例面板。
	UCC_MenuButton* AddButton(UUserWidget* Owner, UVerticalBox* Panel, FName Name, const FText& Label)
	{
		UCC_MenuButton* Button = CreateWidget<UCC_MenuButton>(Owner, UCC_MenuButton::StaticClass(), Name);
		Button->SetLabel(Label);
		Panel->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 6));
		return Button;
	}
}

// 构建示例按钮，并让鼠标悬停/手柄焦点显示高亮。
void UCC_MenuButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.12f, 0.2f, 0.26f, 1.f));
	Background->SetPadding(FMargin(24.f, 16.f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Background;
	Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetJustification(ETextJustify::Center);
	Background->AddChild(Label);
	OnHovered().AddWeakLambda(this, [Background]() { Background->SetBrushColor(FLinearColor(0.22f, 0.38f, 0.46f)); });
	OnUnhovered().AddWeakLambda(this, [Background]() { Background->SetBrushColor(FLinearColor(0.12f, 0.2f, 0.26f)); });
}
// 更新示例按钮显示文字。
void UCC_MenuButton::SetLabel(const FText& Text) { if (Label) Label->SetText(Text); }

// 构建暂停菜单，继续按钮关闭本页，退出按钮打开 Modal 层确认框。
void UCC_PauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* Panel = BuildPanel(WidgetTree, NSLOCTEXT("CCUI", "Paused", "Paused"), false);
	UCC_MenuButton* Resume = AddButton(this, Panel, TEXT("Resume"), NSLOCTEXT("CCUI", "Resume", "Resume"));
	Resume->OnClicked().AddWeakLambda(this, [this]()
	{
		if (UCC_UIController* Controller = GetScreenController()) Controller->RequestClose();
	});
	UCC_MenuButton* Quit = AddButton(this, Panel, TEXT("Quit"), NSLOCTEXT("CCUI", "Quit", "Quit game"));
	Quit->OnClicked().AddWeakLambda(this, [this]()
	{
		if (UCC_PauseMenuController* Controller = Cast<UCC_PauseMenuController>(GetScreenController())) Controller->RequestQuit();
	});
	// 将已加入页面控件树的继续按钮作为默认焦点。
	DefaultFocusWidgetName = TEXT("Resume");
}

// 构建确认弹窗，取消只关闭本页，确认才调用退出游戏。
void UCC_QuitDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* Panel = BuildPanel(WidgetTree, NSLOCTEXT("CCUI", "ConfirmQuit", "Quit this game?"), true);
	UCC_MenuButton* Cancel = AddButton(this, Panel, TEXT("Cancel"), NSLOCTEXT("CCUI", "Cancel", "Cancel"));
	Cancel->OnClicked().AddWeakLambda(this, [this]()
	{
		if (UCC_UIController* Controller = GetScreenController()) Controller->RequestClose();
	});
	UCC_MenuButton* Confirm = AddButton(this, Panel, TEXT("Confirm"), NSLOCTEXT("CCUI", "Confirm", "Quit"));
	Confirm->OnClicked().AddWeakLambda(this, [this]()
	{
		if (UCC_QuitDialogController* Controller = Cast<UCC_QuitDialogController>(GetScreenController())) Controller->ConfirmQuit();
	});
	DefaultFocusWidgetName = TEXT("Cancel");
}

UCC_PauseMenuWidget::UCC_PauseMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_PauseMenuController::StaticClass();
}

UCC_QuitDialogWidget::UCC_QuitDialogWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_QuitDialogController::StaticClass();
}

bool UCC_PauseMenuController::RequestQuit()
{
	return CanHandleActions() && GetRootLayout()->ShowScreen(CCTags::UILayer::Modal, UCC_QuitDialogWidget::StaticClass()) != nullptr;
}

void UCC_QuitDialogController::ConfirmQuit()
{
	if (CanHandleActions()) UKismetSystemLibrary::QuitGame(this, GetPlayerController(), EQuitPreference::Quit, false);
}
