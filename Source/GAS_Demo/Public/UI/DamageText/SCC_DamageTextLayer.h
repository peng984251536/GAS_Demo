#pragma once

#include "Widgets/Layout/SConstraintCanvas.h"
class UCC_DamageTextWidget;

/**
 * 公共 UMG 布局面板：安排子控件，数字内容由 UMG 的 TextBlock 绘制。
 * 简单模板可共享 Layer ID；资源、字体图集和裁剪仍可能拆批，不能保证一个 Draw Call。
 */
class GAS_DEMO_API SCC_DamageTextLayer : public SConstraintCanvas
{
public:
    SLATE_BEGIN_ARGS(SCC_DamageTextLayer) : _OwnerWidget(nullptr) {}
        SLATE_ARGUMENT(UCC_DamageTextWidget*, OwnerWidget)
    SLATE_END_ARGS()
    /**
     * 创建公共面板并启用一次集中位置更新。
     * @param Args 构造参数，包含 UMG 外壳。
     */
    void Construct(const FArguments& Args);
    /**
     * 由容器统一同步位置，不给每个数字编写 Tick。
     * @param Geometry 当前面板几何。
     * @param CurrentTime 引擎当前时间。
     * @param DeltaTime 本帧经过的秒数。
     */
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
    /**
     * 转交 UMG 子控件绘制；简单模板共享起始层，复杂布局可回退标准 Canvas 层级。
     * @param Args Slate 绘制上下文。
     * @param Geometry 当前容器几何。
     * @param CullingRect 绘制裁剪区域。
     * @param DrawElements 输出绘制元素列表。
     * @param LayerId 起始层级。
     * @param WidgetStyle 父级 Slate 样式。
     * @param bParentEnabled 父级是否启用。
     * @return 绘制完成后的最大层级。
     */
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    /**
     * 全屏容器依靠外部 Overlay 填充，不根据数字位置扩展期望尺寸。
     * @param LayoutScaleMultiplier 布局缩放倍率。
     * @return 零期望尺寸。
     */
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }
private:
    TWeakObjectPtr<UCC_DamageTextWidget> OwnerWidget;
};
