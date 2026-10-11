#include "UI/DamageText/SCC_DamageTextLayer.h"
#include "UI/DamageText/CC_DamageTextWidget.h"
#include "UI/DamageText/CC_DamageTextEntryWidget.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ArrangedWidget.h"
#include "Rendering/DrawElements.h"

/**
 * Slate 只布局，不再使用 MakeText 手工绘制数字。
 * @param Args 构造参数，包含 UMG 外壳。
 */
void SCC_DamageTextLayer::Construct(const FArguments& Args)
{
    OwnerWidget = Args._OwnerWidget;
    SConstraintCanvas::Construct(SConstraintCanvas::FArguments());
    SetCanTick(true);
}

/**
 * 一个面板集中更新位置，模板/字体配置变化也在这里同步。
 * @param Geometry 当前面板几何。
 * @param CurrentTime 引擎当前时间。
 * @param DeltaTime 本帧经过的秒数。
 */
void SCC_DamageTextLayer::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
    SConstraintCanvas::Tick(Geometry, CurrentTime, DeltaTime);
    if (auto* Widget = OwnerWidget.Get())
    {
        UClass* DesiredClass = Widget->EntryWidgetClass.Get();
        if (!DesiredClass || DesiredClass->HasAnyClassFlags(CLASS_Abstract)) DesiredClass = UCC_DamageTextEntryWidget::StaticClass();
        if (Widget->CachedEntryClass.Get() != DesiredClass) Widget->SyncWidgets();
        Widget->UpdatePositions(Geometry);
    }
}

/**
 * 使用兼容的 Layer ID 为简单数字保留合批机会；标准 Canvas 可处理复杂遮挡关系。
 * @param Args Slate 绘制上下文。
 * @param Geometry 当前面板几何。
 * @param CullingRect 绘制裁剪区域。
 * @param DrawElements 输出绘制元素列表。
 * @param LayerId 起始层级。
 * @param WidgetStyle 父级 Slate 样式。
 * @param bParentEnabled 父级是否启用。
 * @return 子控件绘制后的最大层级。
 */
int32 SCC_DamageTextLayer::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    const auto* Widget = OwnerWidget.Get();
    if (!Widget || !Widget->bShareDrawLayer)
        return SConstraintCanvas::OnPaint(Args, Geometry, CullingRect, DrawElements, LayerId, WidgetStyle, bParentEnabled);
    FArrangedChildren Arranged(EVisibility::Visible);
    ArrangeChildren(Geometry, Arranged);
    const FPaintArgs ChildArgs = Args.WithNewParent(this);
    int32 MaxLayer = LayerId;
    for (int32 Index = 0; Index < Arranged.Num(); ++Index)
    {
        const auto& Child = Arranged[Index];
        if (!IsChildWidgetCulled(CullingRect, Child))
            MaxLayer = FMath::Max(MaxLayer, Child.Widget->Paint(ChildArgs, Child.Geometry, CullingRect,
                DrawElements, LayerId, WidgetStyle, ShouldBeEnabled(bParentEnabled)));
    }
    return MaxLayer;
}
