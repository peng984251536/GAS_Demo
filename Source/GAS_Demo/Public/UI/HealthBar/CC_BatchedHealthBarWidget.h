#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HealthBar/CC_HealthBarTypes.h"
#include "CC_BatchedHealthBarWidget.generated.h"

class UCanvasPanel;
class UCC_UIController;
class UCC_HealthBarOverlayController;

/**
 * 头顶血条层：每个本地玩家一个全屏实例，负责把"单条血条控件"摆到各个敌人头顶。
 * 本控件自己不决定血条长什么样——外观由单条控件决定（实现 CC_HealthBarItem 接口的任意 UserWidget）。
 *
 * 数据流：血条子系统 → CC_HealthBarOverlayController → CC_HealthBarOverlayModel → 本控件。
 * 本控件只读模型，不访问子系统或 GAS；每帧投影锚点、更新单条控件位置，数据变化时调用 On Health Bar Updated。
 *
 * 单条控件类的选择顺序：角色配置的 Item Widget Class → 本层的 Default Item Widget Class。
 * 单条控件用对象池复用：离开画面、超出距离、死亡隐藏或角色移除时回收，再出现时重新分配。
 *
 * 默认由 CC_RootLayout 的世界覆盖层自动创建（见 HealthBarLayerClass），无需手动 Add to Player Screen。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_BatchedHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 设置不拦截输入，并指定默认控制器与默认单条控件。 */
	UCC_BatchedHealthBarWidget(const FObjectInitializer& ObjectInitializer);
	/** 屏幕方向偏移，单位为 HUD 布局单位；锚点投影之后再应用，负 Y 表示上移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector2D ScreenOffset = FVector2D(0, -12);
	/** 角色配置未指定单条控件时使用；必须实现 CC_HealthBarItem 接口。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar", meta=(MustImplement="/Script/GAS_Demo.CC_HealthBarItem"))
	TSubclassOf<UUserWidget> DefaultItemWidgetClass;
	/** 本层的控制器；可在 C++ 子类中替换显示规则。 */
	UFUNCTION(BlueprintPure, Category="UI|Architecture")
	UCC_UIController* GetOverlayController() const { return OverlayController; }
	/** 当前正在显示的单条控件数量，供调试。 */
	UFUNCTION(BlueprintPure, Category="Health Bar")
	int32 GetActiveItemCount() const { return ActiveItems.Num(); }
protected:
	/** 找到或创建放单条控件的 Canvas。 */
	virtual void NativeOnInitialized() override;
	/** 创建并激活控制器（设计器预览中不创建）。 */
	virtual void NativeConstruct() override;
	/** 回收全部单条控件并释放控制器。 */
	virtual void NativeDestruct() override;
	/** 投影、分配/回收单条控件、更新位置与数据。 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** 控制器类，决定数据来源和显示规则。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Architecture")
	TSubclassOf<UCC_HealthBarOverlayController> ControllerClass;
	/**
	 * 放单条控件的画布。派生蓝图可以自己放一个名为 ItemCanvas 的 Canvas Panel（例如外面再包一层做整体效果）；
	 * 没有时：设计树为空则自动创建，根控件本身是 Canvas Panel 则直接使用。
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Health Bar", meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> ItemCanvas;
private:
	/** 正在显示的一条：控件与上次推送的数据（数据不变时不重复调用蓝图事件）。 */
	struct FActiveItem
	{
		TWeakObjectPtr<UUserWidget> Widget;
		FCC_HealthBarItemData LastData;
		bool bHasData = false;
		bool bSeenThisFrame = false;
	};
	/** 从对象池取出或新建一个单条控件，并放进画布。 */
	UUserWidget* AcquireItem(UClass* ItemClass);
	/** 通知控件释放、隐藏并放回对象池。 */
	void ReleaseItem(UUserWidget* Widget);
	/** 回收全部正在显示的单条控件。 */
	void ReleaseAllItems();
	/** Slate 存在期间持有；NativeDestruct 时释放。 */
	UPROPERTY(Transient)
	TObjectPtr<UCC_UIController> OverlayController;
	/** 按角色索引的正在显示的控件；控件本身由画布持有。 */
	TMap<TWeakObjectPtr<AActor>, FActiveItem> ActiveItems;
	/** 按控件类分组的空闲控件；仍是画布的子控件（已折叠），因此不会被回收。 */
	TMap<TWeakObjectPtr<UClass>, TArray<TWeakObjectPtr<UUserWidget>>> FreeItems;
};
