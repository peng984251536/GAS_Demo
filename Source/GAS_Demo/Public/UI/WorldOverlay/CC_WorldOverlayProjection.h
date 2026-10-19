// 世界覆盖层共用的投影换算：世界坐标 → 视口像素 → 本地玩家 HUD 布局坐标 → 绘制控件局部坐标。
#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class APlayerController;

/**
 * 每次 Paint 构建一次，整批条目复用同一份相机投影。
 * 处理了窗口偏移（窗口化 / PIE）、DPI 缩放、分屏和保持宽高比产生的黑边。
 * 头顶血条和伤害飘字都用它，坐标问题只需在这里修。
 */
struct GAS_DEMO_API FCC_WorldOverlayProjector
{
	/** 读取本地玩家当前相机和视口；没有本地玩家、视口或画面尺寸为 0 时返回 false，此时不要投影。 */
	bool Initialize(APlayerController* PlayerController, const FGeometry& WidgetGeometry);

	/**
	 * 世界坐标 → 本地玩家 HUD 布局坐标（尚未转换到绘制控件）。
	 * 位于相机背后时返回 false；bRequireInsideView 为 true 时，落在画面外（含黑边）也返回 false。
	 * 屏幕方向的偏移（错位、上升高度等）应在这个坐标系里叠加，单位不随分辨率变化。
	 */
	bool ProjectToPlayerLocal(const FVector& WorldLocation, FVector2D& OutPlayerLocal, bool bRequireInsideView) const;

	/** HUD 布局坐标 → 绘制控件局部坐标，可直接用于 ToPaintGeometry。 */
	FVector2D PlayerLocalToWidget(const FVector2D& PlayerLocal) const;

	/** 相机位置，用于按距离剔除。 */
	const FVector& GetViewOrigin() const { return ViewOrigin; }

private:
	FMatrix ViewProjection = FMatrix::Identity;
	/** 保持宽高比后扣除黑边的实际画面范围（视口像素）。 */
	FIntRect ViewRect;
	/** 该玩家的视口范围；分屏时只是窗口的一部分。 */
	FIntRect PlayerRect;
	/** 本地玩家 HUD 区域的几何，用于从 HUD 布局坐标换到 Slate 绝对坐标。 */
	FGeometry PlayerGeometry;
	/** 绘制控件自身的几何。 */
	FGeometry WidgetGeometry;
	FVector2D PixelToLocal = FVector2D::UnitVector;
	FVector ViewOrigin = FVector::ZeroVector;
};
