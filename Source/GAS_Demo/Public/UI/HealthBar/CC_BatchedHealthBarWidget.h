#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CC_BatchedHealthBarWidget.generated.h"

/**
 * 集中绘制头顶血条的 HUD 层，每个本地玩家只创建一个全屏实例。
 * 蓝图继承此类后用 Create Widget + Add to Player Screen 创建，不需要挂到 Actor 上。
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
protected:
	/** 共用一次相机投影数据，剔除不可见条目后按三个固定层级批量绘制。 */
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
};
