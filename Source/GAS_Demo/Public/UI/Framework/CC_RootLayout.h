#pragma once

#include "CommonActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CC_RootLayout.generated.h"

class UOverlay;
class UCC_ActivatableWidget;

/** 官方 CommonUI 栈的薄封装，只开放内置过渡配置，不另写动画状态机。 */
UCLASS()
class GAS_DEMO_API UCC_UIStack : public UCommonActivatableWidgetStack
{
	GENERATED_BODY()
public:
	/** Slate 构建前设置过渡类型、秒数、曲线和对象池释放策略。 */
	void Configure(ECommonSwitcherTransition Type, float Duration);
};

/** 层的行为类型：决定过渡动画、是否暂停世界、是否阻挡下层点击。 */
UENUM(BlueprintType)
enum class ECC_UILayerKind : uint8
{
	Game,  // HUD：即时切换，不暂停，不阻挡下层。
	Menu,  // 菜单：使用 MenuTransition，有页面时暂停单机世界并阻挡下层点击。
	Modal  // 弹窗：使用 ModalTransition，其余同 Menu。
};

/** 根布局的一层配置；数组顺序即视觉顺序（下标 0 在最底层）。 */
USTRUCT(BlueprintType)
struct FCC_UILayerConfig
{
	GENERATED_BODY()
	FCC_UILayerConfig() = default;
	FCC_UILayerConfig(FGameplayTag InTag, ECC_UILayerKind InKind) : Tag(InTag), Kind(InKind) {}
	/** 层标签，ShowScreen 按它查找容器；同一标签只能出现一次。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI", meta=(Categories="UI.Layer"))
	FGameplayTag Tag;
	/** 该层的行为类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	ECC_UILayerKind Kind = ECC_UILayerKind::Menu;
};

/** 运行时的一层：标签、类型与对应的官方栈。 */
USTRUCT()
struct FCC_UIRuntimeLayer
{
	GENERATED_BODY()
	FGameplayTag Tag;
	ECC_UILayerKind Kind = ECC_UILayerKind::Menu;
	UPROPERTY(Transient) TObjectPtr<UCC_UIStack> Stack;
	/** 菜单与弹窗层有页面时暂停世界并阻挡下层点击。 */
	bool IsMenuLike() const { return Kind != ECC_UILayerKind::Game; }
};

/** 某层的当前显示页面变化时广播；Screen 为空表示该层没有页面。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_UILayerChanged, FGameplayTag, Layer, UCommonActivatableWidget*, Screen);

/** 对应 Lyra PrimaryGameLayout：Policy 按 LocalPlayer 持有四层根布局，切图清空内容但复用根对象。 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_RootLayout : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	/** 根布局保持激活，提供无菜单时的游戏输入回退。 */
	UCC_RootLayout(const FObjectInitializer& ObjectInitializer);
	/** 最后一个菜单关闭后，恢复 Game 模式和鼠标行为。 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	/**
	 * 打开页面并传数据；过渡/重入/无效请求返回空。
	 * 同层同类页面已存在时返回该实例：传入非空 Context 会替换旧上下文并再次触发 On Screen Opened，
	 * 但被同层其他页面覆盖的实例不会移到栈顶（会输出警告）。
	 */
	UFUNCTION(BlueprintCallable, Category="UI", meta=(DeterminesOutputType="ScreenClass", Categories="UI.Layer"))
	UCC_ActivatableWidget* ShowScreen(FGameplayTag Layer, TSubclassOf<UCC_ActivatableWidget> ScreenClass, UObject* Context = nullptr);
	/** 关闭最上层且允许关闭的页面；退场动画和下页恢复由官方栈完成。 */
	UFUNCTION(BlueprintCallable, Category="UI")
	bool CloseScreen(UCC_ActivatableWidget* Screen);
	/** 从最高层往下找到当前显示页面并请求关闭。 */
	UFUNCTION(BlueprintCallable, Category="UI")
	bool CloseTopScreen();
	/** 返回层级最高的当前显示页面；过渡期间可能仍是退出中的旧页面。 */
	UFUNCTION(BlueprintPure, Category="UI")
	UCommonActivatableWidget* GetTopScreen() const;
	/** 是否还有容器在播放过渡动画。 */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsTransitioning() const { return !TransitionTokens.IsEmpty(); }
	/** 一旦清理开始即永久拒绝该根布局的新页面。 */
	bool IsShuttingDown() const { return bShuttingDown; }
	/** 异步任务在动画/同步栈修改期间等待下一帧，避免加载完成时丢失导航请求。 */
	bool CanAcceptScreen() const { return !bShuttingDown && !bDetached && !bChangingStack && !IsTransitioning(); }
	/** 跨图时从 GameInstance 获取当前世界，不缓存创建时的旧世界。 */
	virtual UWorld* GetWorld() const override;
	/** 清空地图相关页面与请求，保留根布局及其四个容器供重新挂载。 */
	void ResetWorldContent();
	/** 切图时移除视口并释放 Slate/页面池引用，保留 WidgetTree 中的四层容器。 */
	void DetachFromViewport() { ResetWorldContent(); RemoveFromParent(); ReleaseSlateResources(true); }
	/** Policy 完成挂载后解除切图保护；最终 Shutdown 后不能恢复。 */
	void ResumeLayout() { if (!bShuttingDown) bDetached = false; }
	/** 根布局是否处于可导航的挂载阶段。 */
	bool IsLayoutAttached() const { return !bDetached; }
	/** 加载或栈动画期间禁止用户导航；异步入栈自身只检查 CanAcceptScreen。 */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsInputBlocked() const { return IsTransitioning() || !AsyncInputTokens.IsEmpty(); }
	/** 异步请求获得独立输入锁；返回的令牌必须交还给 ResumeAsyncInput。 */
	FName SuspendAsyncInput();
	/** 只释放该请求的令牌，多请求并发时不会提前恢复输入。 */
	void ResumeAsyncInput(FName Token);
	/** 清理开始时通知所有异步页面请求立即取消。 */
	FSimpleMulticastDelegate OnShutdown;
	/** Menu/Modal 是否有页面，包含正在进出场的页面。 */
	UFUNCTION(BlueprintPure, Category="UI")
	bool HasMenu() const;
	/** 显示不抢输入的普通 UserWidget；Duration 按游戏时间计时，<=0 时手动移除。 */
	UFUNCTION(BlueprintCallable, Category="UI", meta=(DeterminesOutputType="WidgetClass"))
	UUserWidget* ShowNotification(TSubclassOf<UUserWidget> WidgetClass, float Duration = 3.0f);
	/** 移除属于本布局的提示，并清除它的自动移除计时器。 */
	UFUNCTION(BlueprintCallable, Category="UI")
	void DismissNotification(UUserWidget* Widget);
	/** 玩家退出或 GameInstance 结束时永久清理；切图应使用 ResetWorldContent。 */
	void Shutdown();
	/** 精确按 Layer 标签查找对应的栈，未配置的标签返回空。 */
	UCommonActivatableWidgetContainerBase* GetLayer(FGameplayTag Layer) const;
	/** 外部订阅此事件响应页面变化，不需要每帧轮询。 */
	UPROPERTY(BlueprintAssignable, Category="UI")
	FCC_UILayerChanged OnLayerChanged;

protected:
	/** 构建四层栈、输入遮罩和提示容器，然后触发蓝图初始化。 */
	virtual void NativeOnInitialized() override;
	/** Slate 释放时清空地图内容，不把可复用布局永久 Shutdown。 */
	virtual void NativeDestruct() override;
	/** 菜单/弹窗过渡秒数；设为 0 即时切换。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Transition", meta=(ClampMin="0", ClampMax="1"))
	float TransitionDuration = 0.18f;
	/** 菜单层默认横向切换，可在根布局蓝图类默认值中修改。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Transition")
	ECommonSwitcherTransition MenuTransition = ECommonSwitcherTransition::Horizontal;
	/** 弹窗层默认淡入淡出。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Transition")
	ECommonSwitcherTransition ModalTransition = ECommonSwitcherTransition::FadeOnly;
	/**
	 * 层级表，下标 0 在最底层。留空时使用内置四层：
	 * UI.Layer.Game(Game) → UI.Layer.GameMenu(Menu) → UI.Layer.Menu(Menu) → UI.Layer.Modal(Modal)。
	 * 新增一层只需在根布局蓝图默认值里加一行，并在 DefaultGameplayTags.ini 注册对应标签。
	 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Layers", meta=(TitleProperty="Tag"))
	TArray<FCC_UILayerConfig> Layers;
	/** 用 SafeZone 包住全部层，避免主机/移动端的 HUD 和菜单被屏幕边缘裁切；PC 上通常无影响。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Layers")
	bool bApplySafeZone = true;
	/** 单机打开菜单/弹窗时是否暂停世界；需要实时背包时可关闭。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Input")
	bool bPauseGameWhileMenuOpen = true;

private:
	/** 按容器登记/移除输入过滤令牌，结束时请求恢复焦点。 */
	void HandleTransition(UCommonActivatableWidgetContainerBase* Stack, bool bTransitioning);
	/** 当前显示页面变化后更新命中测试并广播层级事件。 */
	void HandleDisplayed(UCommonActivatableWidget* Screen, FGameplayTag Layer);
	/** 同步下层命中测试、过渡遮罩以及单机暂停。 */
	void UpdateInteraction();
	/** 解除仅由本布局持有的全部输入过滤。 */
	void ReleaseInputTokens();

	/** 该类型层的过渡时长：Game 层即时切换，其余使用 TransitionDuration。 */
	float GetLayerDuration(ECC_UILayerKind Kind) const { return Kind == ECC_UILayerKind::Game ? 0.f : TransitionDuration; }

	/** 按视觉顺序（下标 0 最底层）排列的运行时层。 */
	UPROPERTY(Transient) TArray<FCC_UIRuntimeLayer> RuntimeLayers;
	/** 最上视觉层：提示，其自身和子控件均不参与命中测试。 */
	UPROPERTY(Transient) TObjectPtr<UOverlay> NotificationLayer;
	/** 动画期间拦截鼠标点击的透明全屏遮罩。 */
	UPROPERTY(Transient) TObjectPtr<UWidget> InputShield;
	/** 容器到过滤令牌的映射，防止一个容器提前解锁其他容器。 */
	TMap<UCommonActivatableWidgetContainerBase*, FName> TransitionTokens;
	/** 软引用页面加载持有的输入锁，与动画令牌独立。 */
	TSet<FName> AsyncInputTokens;
	uint32 AsyncRequestSerial = 0;
	/** 临时提示到移除计时器的映射；弱引用不延长控件生命周期。 */
	TMap<TWeakObjectPtr<UUserWidget>, FTimerHandle> NotificationTimers;
	/** 清理期间禁止回调重新打开页面。 */
	bool bShuttingDown = false;
	/** 临时脱离视口时禁止导航，但不是永久销毁。 */
	bool bDetached = true;
	/** 防止页面初始化/显示事件同步重入栈修改。 */
	bool bChangingStack = false;
	/** 标记本 UI 发起的暂停，避免解除其他系统预先施加的暂停。 */
	bool bPausedWorld = false;
};
