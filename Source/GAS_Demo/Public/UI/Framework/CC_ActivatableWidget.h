#pragma once

#include "CommonActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "CC_ActivatableWidget.generated.h"

class UCC_UIController;
class UCC_UIModel;

/** 页面声明的输入需求，由 CommonUI 的 ActionRouter 统一应用。 */
UENUM(BlueprintType)
enum class ECC_UIInputMode : uint8
{
	Game,        // 仅游戏输入，适用于 HUD。
	GameAndMenu, // 游戏与 UI 都可接收输入。
	Menu         // 仅 UI 输入，并阻止角色移动和视角输入。
};

/** 通用页面基类：声明输入、焦点和关闭策略；页面栈负责激活、失活及复用。 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_ActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	/** 默认启用返回操作与官方焦点恢复。 */
	UCC_ActivatableWidget(const FObjectInitializer& ObjectInitializer);
	/** 页面成为输入目标时，向 ActionRouter 提供完整输入配置。 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	/** 请求关闭本页；被覆盖、过渡中或受保护时返回 false。 */
	UFUNCTION(BlueprintCallable, Category="UI")
	bool CloseScreen();
	/** 是否允许正常关闭；强制切图清理不受此限制。 */
	UFUNCTION(BlueprintPure, Category="UI")
	bool CanCloseScreen() const { return bAllowClose; }
	/** 动画结束后请求官方输入树刷新焦点，不直接抢焦点。 */
	void RefreshNavigationFocus() { RequestRefreshFocus(); }
	/** 每次入栈前注入数据并重置旧焦点；对象池复用也执行。 */
	void PrepareForDisplay(UObject* InContext);
	/** 获取本页独立的业务控制器；未配置或入栈前可能为空。 */
	UFUNCTION(BlueprintPure, Category="UI|Architecture")
	UCC_UIController* GetScreenController() const { return ScreenController; }
	/** 获取本页只用于展示的模型。 */
	UFUNCTION(BlueprintPure, Category="UI|Architecture")
	UCC_UIModel* GetUIModel() const;

protected:
	/** 在蓝图激活事件前启用控制器并推送模型快照。 */
	virtual void NativeOnActivated() override;
	/** 在官方栈响应失活前解除控制器和模型监听。 */
	virtual void NativeOnDeactivated() override;
	/** C++ 展示适配点；不在此调用业务系统或发起导航。 */
	virtual void NativeOnModelChanged();
	/** 模型变化时刷新蓝图控件；首次激活也推送一次。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Architecture")
	void OnUIModelChanged(UCC_UIModel* Model);
	/** 可选功能控制器类；小型纯展示页面可留空。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Architecture")
	TSubclassOf<UCC_UIController> ControllerClass;
	/** 消费返回键；允许蓝图先处理未保存提示等业务，再决定是否关闭。 */
	virtual bool NativeOnHandleBackAction() override;
	/** Slate 重建时设置导航边界，避免焦点移动到下层页面。 */
	virtual void NativeConstruct() override;
	/** Slate 释放/返回对象池时清除上下文，不等同于 UObject 销毁。 */
	virtual void NativeDestruct() override;
	/** 优先使用蓝图指定的焦点，否则查找默认名称对应的控件。 */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** 页面激活时所需的输入模式。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Input")
	ECC_UIInputMode InputConfig = ECC_UIInputMode::Menu;
	/** Game/GameAndMenu 模式的鼠标捕获方式；Menu 始终不捕获。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
	/** 是否允许返回键关闭；主菜单底页可关闭此选项。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Input")
	bool bAllowBack = true;
	/** 是否允许通过 CloseScreen 关闭；常驻 HUD 设为 false。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Lifecycle")
	bool bAllowClose = true;
	/** ShowScreen 传入的业务数据；本页持有到出栈释放 Slate 为止。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI|Data")
	TObjectPtr<UObject> ScreenContext;
	/** 入栈前初始化数据；这里不要同步发起下一次页面导航。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Lifecycle", meta=(DisplayName="On Screen Opened"))
	void BP_OnScreenOpened(UObject* Context);
	/** 默认焦点控件名；蓝图 Get Desired Focus Target 的结果优先。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Input")
	FName DefaultFocusWidgetName;

private:
	/** 创建本次入栈会话的控制器；不在构造函数中访问玩家或业务系统。 */
	void EnsureController();
	/** 模型变化转交展示回调，失活期间忽略通知。 */
	UFUNCTION()
	void HandleModelChanged();
	/** 当前页面拥有控制器；释放 Slate 时断开，池化复用时重新创建。 */
	UPROPERTY(Transient)
	TObjectPtr<UCC_UIController> ScreenController;
};
