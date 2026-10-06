// 集中绘制的 Slate 叶子控件：所有数字共用一份画布，不为每次命中创建子控件。
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UI/DamageText/CC_DamageTextTypes.h"

class APlayerController;

/**
 * 伤害飘字绘制层。
 *
 * 这是一个 SLeafWidget（叶子控件，没有子槽），在 OnPaint 里一次性把所有
 * 活动飘字画完。所有数字共用一个控件的一次 OnPaint，但会提交多个文字绘制元素，不保证一个 draw call。
 *
 * 性能要点：
 *   - 不创建任何子控件。飘字数量增加不会增加控件数量，也就没有额外的布局计算。
 *   - 每帧只构建一次视图/投影矩阵，整批飘字复用，而不是每个飘字算一遍。
 *   - ComputeDesiredSize 返回 Zero，控件不占布局空间，纯粹作为绘制画布。
 *   - 没有活动飘字时 OnPaint 立即返回，不做任何投影。
 *
 * 构造时必须传入 PlayerController，用于取摄像机位置和视口尺寸。
 */
class GAS_DEMO_API SCC_DamageTextLayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCC_DamageTextLayer)
		: _FontSize(28.0f)
		, _CritFontSize(40.0f)
		, _RiseHeight(80.0f)
	{}
		/** 普通伤害字号。用 TAttribute 惰性求值，编辑器里改属性立即生效，无需重建控件。 */
		SLATE_ATTRIBUTE(float, FontSize)
		/** 暴击字号。应当明显大于普通字号才能形成视觉层级。 */
		SLATE_ATTRIBUTE(float, CritFontSize)
		/** 整个生命周期内向上飘升的像素高度。 */
		SLATE_ATTRIBUTE(float, RiseHeight)
		/** 投影用的控制器。通常是本机玩家的 Controller。 */
		SLATE_ARGUMENT(APlayerController*, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// --- SWidget ---
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	/** 按样式取颜色；Alpha 用于生命周期淡入淡出。 */
	static FLinearColor StyleColor(ECC_DamageTextStyle Style, float Alpha);

	/** 按样式取字号（未乘弹跳缩放）。 */
	float StyleFontSize(ECC_DamageTextStyle Style) const;

	/** 格式化数值文本：伤害为纯数字，治疗带 + 号。 */
	static FText FormatAmount(float Amount, ECC_DamageTextStyle Style);

	/** 弱引用，避免绘制层延长控制器生命周期。 */
	TWeakObjectPtr<APlayerController> CachedController;

	TAttribute<float> NormalFontSize;
	TAttribute<float> CriticalFontSize;
	TAttribute<float> Rise;
};
