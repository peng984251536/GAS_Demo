#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "Engine/StreamableManager.h"
#include "CC_WorldIndicatorLayer.generated.h"

class UCanvasPanel;
class UCC_WorldIndicator;
class UCC_WorldIndicatorSubsystem;

/** 一个正在显示的指示器对应的控件。 */
USTRUCT()
struct FCC_WorldIndicatorWidgetEntry
{
	GENERATED_BODY()
	UPROPERTY(Transient) TObjectPtr<UUserWidget> Widget;
	/** 上一帧是否显示，只在状态变化时才改可见性。 */
	bool bShown = false;
};

/**
 * 世界指示器的显示层，每个本地玩家一个，由 CC_RootLayout 在世界覆盖层中创建（位于所有页面之下，菜单打开时隐藏）。
 *
 *  - 增删：订阅 UCC_WorldIndicatorSubsystem，增删时从 FUserWidgetPool 取出 / 归还控件，调用 Bind / Unbind。
 *  - 每帧：构建一次投影，对每个指示器 SetRenderTranslation；出画面、超出距离、隐藏时设为 Hidden。
 *    Slot 位置固定为 (0,0)，移动只改渲染平移，不触发重新布局。
 *
 * 本层不认识任何具体 UI 类型；新增一种世界空间 UI 只需要一个实现 CC World Indicator Widget 的控件和一个数据来源。
 */
UCLASS()
class GAS_DEMO_API UCC_WorldIndicatorLayer : public UUserWidget
{
	GENERATED_BODY()
public:
	UCC_WorldIndicatorLayer(const FObjectInitializer& ObjectInitializer);
	/** 当前显示中的控件数量，供调试。 */
	UFUNCTION(BlueprintPure, Category="UI|World Indicator")
	int32 GetActiveWidgetCount() const { return ActiveWidgets.Num(); }
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void HandleIndicatorAdded(UCC_WorldIndicator* Indicator);
	void HandleIndicatorRemoved(UCC_WorldIndicator* Indicator);
	/** 控件类已加载时立即创建；否则异步加载后再创建（期间指示器若被移除则放弃）。 */
	void ShowIndicator(UCC_WorldIndicator* Indicator);
	void CreateWidgetFor(UCC_WorldIndicator* Indicator, UClass* WidgetClass);
	void HideIndicator(UCC_WorldIndicator* Indicator);
	void ReleaseAll();

	/** 放控件的画布，根控件。 */
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	/** 引擎自带的 UserWidget 对象池，按控件类复用。 */
	UPROPERTY(Transient) FUserWidgetPool WidgetPool;
	UPROPERTY(Transient) TMap<TObjectPtr<UCC_WorldIndicator>, FCC_WorldIndicatorWidgetEntry> ActiveWidgets;
	TWeakObjectPtr<UCC_WorldIndicatorSubsystem> BoundSubsystem;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	/** 正在异步加载控件类的指示器。 */
	TMap<TWeakObjectPtr<UCC_WorldIndicator>, TSharedPtr<FStreamableHandle>> PendingLoads;
};
