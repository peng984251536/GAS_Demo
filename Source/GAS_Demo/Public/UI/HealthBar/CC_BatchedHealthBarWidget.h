#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CC_BatchedHealthBarWidget.generated.h"

class UCC_UIController;
class UCC_HealthBarOverlayController;

/**
 * 集中绘制头顶血条的视图层，每个本地玩家只需一个全屏实例。
 * 数据流：血条子系统 → CC_HealthBarOverlayController → CC_HealthBarOverlayModel → 本控件 NativePaint。
 * 本控件只读模型，不访问子系统、角色或 GAS。
 *
 * 默认由 CC_RootLayout 的世界覆盖层自动创建（见 HealthBarLayerClass），无需手动 Add to Player Screen；
 * 想改样式可派生蓝图，再把根布局的 HealthBarLayerClass 指向它。
 * 设计器可保持空白；所有血条由 NativePaint 直接生成绘制元素，不创建逐角色子控件。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_BatchedHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 设置不拦截输入，并允许世界位置变化时持续重绘。 */
	UCC_BatchedHealthBarWidget(const FObjectInitializer& ObjectInitializer);
	/** 所有血条共用的背景颜色，可在派生蓝图默认值中调整。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor BackgroundColor = FLinearColor(0.03f, 0.03f, 0.03f, 0.85f);
	/** 边框颜色；填充颜色由每个角色的 Options.Color 决定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor BorderColor = FLinearColor::Black;
	/** 向血条外侧扩展的边框宽度，单位为 HUD 布局单位；0 表示不绘制边框。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(ClampMin="0")) float BorderWidth = 1.f;
	/** 屏幕方向偏移，单位为 HUD 布局单位；世界锚点投影之后再应用，负 Y 表示上移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector2D ScreenOffset = FVector2D(0, -12);
	/** 本层的控制器；可在 C++ 子类中替换显示规则。 */
	UFUNCTION(BlueprintPure, Category="UI|Architecture")
	UCC_UIController* GetOverlayController() const { return OverlayController; }
protected:
	/** 创建并激活控制器（设计器预览中不创建）。 */
	virtual void NativeConstruct() override;
	/** 释放控制器，解除它对子系统的订阅。 */
	virtual void NativeDestruct() override;
	/** 共用一次相机投影，读取模型快照，按三个固定层级批量绘制。 */
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
	/** 控制器类，决定数据来源和显示规则。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Architecture")
	TSubclassOf<UCC_HealthBarOverlayController> ControllerClass;
private:
	/** Slate 存在期间持有；NativeDestruct 时释放。 */
	UPROPERTY(Transient)
	TObjectPtr<UCC_UIController> OverlayController;
};
