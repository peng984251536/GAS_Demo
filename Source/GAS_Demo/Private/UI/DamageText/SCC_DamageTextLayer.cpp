// 绘制流程：读取飘字展示模型 → 共用视图投影 → 计算淡入淡出与上升 → 提交文字及描边绘制元素。
#include "UI/DamageText/SCC_DamageTextLayer.h"

#include "UI/DamageText/CC_DamageTextController.h"
#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

// 保存样式属性与本地控制器弱引用；样式通过 TAttribute 惰性读取。
void SCC_DamageTextLayer::Construct(const FArguments& InArgs)
{
	NormalFontSize = InArgs._FontSize;
	CriticalFontSize = InArgs._CritFontSize;
	Rise = InArgs._RiseHeight;
	CachedController = InArgs._PlayerController;
	CachedModel = InArgs._Model;

	// 叶子绘制层不参与命中测试，否则会挡住 HUD 上的按钮点击。
	SetCanTick(false);
}

// 画布不主动占用布局大小；在 HUD 的 Overlay/Canvas 槽中需要设置填充或拉伸。
FVector2D SCC_DamageTextLayer::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// 返回零尺寸：本控件是纯绘制画布，不占布局空间，也不干扰其他 UI 的排布。
	return FVector2D::ZeroVector;
}

// 按普通、暴击、治疗选择颜色，再叠加该条目当前的生命周期透明度。
FLinearColor SCC_DamageTextLayer::StyleColor(ECC_DamageTextStyle Style, float Alpha)
{
	// 配色取自项目的低多边形 UI 规范，保证与主菜单/HUD 同源。
	FLinearColor Base;
	switch (Style)
	{
	case ECC_DamageTextStyle::Critical:
		// 暴击：暖橙，与按钮 Hover 强调色一致
		Base = FLinearColor(0.910f, 0.471f, 0.227f);
		break;
	case ECC_DamageTextStyle::Heal:
		// 治疗：青绿，与按钮 Normal 色一致
		Base = FLinearColor(0.239f, 0.490f, 0.537f);
		break;
	case ECC_DamageTextStyle::Normal:
	default:
		// 普通伤害：暖白，任何背景下都可读
		Base = FLinearColor(1.0f, 0.941f, 0.816f);
		break;
	}
	Base.A = Alpha;
	return Base;
}

// 暴击使用独立字号，普通伤害和治疗使用普通字号。
float SCC_DamageTextLayer::StyleFontSize(ECC_DamageTextStyle Style) const
{
	// TAttribute::Get() 在编辑器改属性后立刻返回新值，不需要重建 Slate 控件。
	return Style == ECC_DamageTextStyle::Critical
		? CriticalFontSize.Get()
		: NormalFontSize.Get();
}

// 数值统一取绝对值并四舍五入；是否带加号由治疗样式决定。
FText SCC_DamageTextLayer::FormatAmount(float Amount, ECC_DamageTextStyle Style)
{
	// 一律取整显示。飘字停留时间很短，小数读不出来还更费排版。
	if (Style == ECC_DamageTextStyle::Heal)
	{
		return FText::FromString(
			FString::Printf(TEXT("+%d"), FMath::RoundToInt(FMath::Abs(Amount))));
	}
	return FText::FromString(
		FString::Printf(TEXT("%d"), FMath::RoundToInt(FMath::Abs(Amount))));
}

// 一轮 OnPaint 遍历整批条目，描边与正文复用固定层级；DrawElement 数量不等于最终 GPU 批次数。
int32 SCC_DamageTextLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const UCC_DamageTextModel* Model = CachedModel.Get();
	if (!Model || Model->GetEntries().Num() == 0)
	{
		// 空闲帧立即返回，不做任何投影和字体查询。
		return LayerId;
	}
	const TArray<FCC_DamageTextEntry>& Entries = Model->GetEntries();

	// 每帧只构建一次投影数据，整批飘字复用；换算细节见 FCC_WorldOverlayProjector。
	FCC_WorldOverlayProjector Projector;
	if (!Projector.Initialize(CachedController.Get(), AllottedGeometry))
	{
		return LayerId;
	}

	// 字体度量服务：只取一次，整批复用。
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	const float Lifetime = FMath::Max(Model->GetLifetime(), KINDA_SMALL_NUMBER);

	// 描边偏移。单色文字在明暗变化的场景上会糊掉，四向描边是低成本的解决办法。
	static const FVector2D OutlineOffsets[4] = {
		FVector2D(-1.0f, 0.0f), FVector2D(1.0f, 0.0f),
		FVector2D(0.0f, -1.0f), FVector2D(0.0f, 1.0f)
	};

	for (const FCC_DamageTextEntry& Entry : Entries)
	{
		// --- 世界坐标 -> HUD 布局坐标 ---
		// 飘字允许部分超出画面，只跳过位于摄像机背后的条目。
		FVector2D AnchorLocal;
		if (!Projector.ProjectToPlayerLocal(Entry.WorldLocation, AnchorLocal, false))
		{
			continue;
		}

		// --- 生命周期曲线 ---
		const float T = FMath::Clamp(Entry.Elapsed / Lifetime, 0.0f, 1.0f);

		// 透明度：前 15% 淡入，后 35% 淡出，中间保持全不透明。
		// 这样数字在屏幕上有足够长的可读时间，而不是一直在半透明状态。
		const float Alpha = T < 0.15f
			? T / 0.15f
			: FMath::Clamp((1.0f - T) / 0.35f, 0.0f, 1.0f);

		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 上升：开方曲线，先快后慢，收尾几乎停住，比匀速更符合直觉。
		const float RiseOffset = Rise.Get() * FMath::Sqrt(T);

		// 连击弹跳：合并命中时 Pulse 置 1 并快速衰减，做一次性放大冲击。
		const float FontSize = StyleFontSize(Entry.Style) * (1.0f + Entry.Pulse * 0.18f);

		const FText Label = FormatAmount(Entry.Amount, Entry.Style);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", FontSize);

		const FVector2D TextSize = FontMeasure->Measure(Label, FontInfo);

		// 错位偏移与上升高度都在 HUD 布局坐标里叠加，不随分辨率/DPI 变化。
		const FVector2D PlayerLocal =
			AnchorLocal - FVector2D(Entry.StackOffset.X, Entry.StackOffset.Y + RiseOffset);

		const FVector2D LocalPos = Projector.PlayerLocalToWidget(PlayerLocal);
		const FVector2D DrawPos = LocalPos - TextSize * 0.5f;

		// 治疗不加描边：本身颜色偏浅，描边反而显脏。
		if (Entry.Style != ECC_DamageTextStyle::Heal)
		{
			const FLinearColor OutlineColor(0.0f, 0.0f, 0.0f, Alpha * 0.85f);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(
						TextSize, FSlateLayoutTransform(DrawPos + OutlineOffsets[Index])),
					Label,
					FontInfo,
					ESlateDrawEffect::None,
					OutlineColor);
			}
		}

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(DrawPos)),
			Label,
			FontInfo,
			ESlateDrawEffect::None,
			StyleColor(Entry.Style, Alpha));
	}

	return LayerId + 3;
}
