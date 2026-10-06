#pragma once

#include "CommonButtonBase.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Framework/CC_UIController.h"
#include "CC_DemoScreens.generated.h"

class UTextBlock;

/** 暂停菜单业务入口：继续游戏、请求退出确认。 */
UCLASS()
class GAS_DEMO_API UCC_PauseMenuController : public UCC_UIController
{
	GENERATED_BODY()
public:
	/** 在顶层交互有效时打开退出确认框。 */
	UFUNCTION(BlueprintCallable, Category="UI|Pause")
	bool RequestQuit();
};

/** 退出确认业务入口：只有顶层确认页才能实际退出游戏。 */
UCLASS()
class GAS_DEMO_API UCC_QuitDialogController : public UCC_UIController
{
	GENERATED_BODY()
public:
	/** 执行用户明确确认的退出操作。 */
	UFUNCTION(BlueprintCallable, Category="UI|Quit")
	void ConfirmQuit();
};

/** 原生 CommonButton 示例：用于验证交互与焦点，实际游戏可使用自己的按钮蓝图。 */
UCLASS()
class GAS_DEMO_API UCC_MenuButton : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	/** 设置按钮显示文字。 */
	void SetLabel(const FText& Text);
protected:
	/** 构建文字和背景，绑定悬停/焦点高亮。 */
	virtual void NativeOnInitialized() override;
private:
	/** 按钮的文字控件，由内部 WidgetTree 持有。 */
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};

/** 最小暂停菜单示例：继续游戏，或打开退出确认框。 */
UCLASS()
class GAS_DEMO_API UCC_PauseMenuWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** 指定暂停菜单控制器，界面按钮只转发用户意图。 */
	UCC_PauseMenuWidget(const FObjectInitializer& ObjectInitializer);
protected:
	/** 构建按钮、绑定操作，并把默认焦点设置为继续游戏。 */
	virtual void NativeOnInitialized() override;
};

/** Modal 层退出确认框示例，取消后恢复下层页面和焦点。 */
UCLASS()
class GAS_DEMO_API UCC_QuitDialogWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** 指定确认框控制器。 */
	UCC_QuitDialogWidget(const FObjectInitializer& ObjectInitializer);
protected:
	/** 构建确认/取消按钮，默认焦点放在取消按钮以避免误退出。 */
	virtual void NativeOnInitialized() override;
};
