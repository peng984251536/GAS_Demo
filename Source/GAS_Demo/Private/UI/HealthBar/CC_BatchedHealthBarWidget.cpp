#include "UI/HealthBar/CC_BatchedHealthBarWidget.h"
#include "UI/HealthBar/CC_HealthBarOverlayController.h"
#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

UCC_BatchedHealthBarWidget::UCC_BatchedHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 全屏 UI 只负责显示，不阻挡鼠标点击或 HUD 按钮。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// 角色和相机移动不一定触发 UMG 属性变化；设为易变控件，避免失效缓存导致血条位置停住。
	ForceVolatile(true);
	ControllerClass = UCC_HealthBarOverlayController::StaticClass();
}

// 每次 Slate 构建开始一次控制器会话；切图重新挂载时会得到新的会话。
void UCC_BatchedHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime() || OverlayController) return;
	OverlayController = UCC_UIController::CreateForView(this, ControllerClass);
	if (OverlayController) OverlayController->Activate();
}

void UCC_BatchedHealthBarWidget::NativeDestruct()
{
	if (OverlayController) OverlayController->Release();
	OverlayController = nullptr;
	Super::NativeDestruct();
}

int32 UCC_BatchedHealthBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& DrawElements, int32 LayerId,
	const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	// 保留父类及蓝图已有内容的最高层级，血条从其上方继续绘制。
	const int32 BaseLayer = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, WidgetStyle, bParentEnabled);
	const UCC_HealthBarOverlayModel* Model = OverlayController ? Cast<UCC_HealthBarOverlayModel>(OverlayController->GetModel()) : nullptr;
	if (!Model || Model->GetItems().IsEmpty()) return BaseLayer;
	// 整批血条共用一份相机投影，不为每个角色单独构建投影数据。
	FCC_WorldOverlayProjector Projector;
	if (!Projector.Initialize(GetOwningPlayer(), Geometry)) return BaseLayer;
	// 共用白色纹理资源，通过顶点颜色控制样式，避免逐角色材质打断合批。
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FLinearColor Tint = WidgetStyle.GetColorAndOpacityTint();
	const float Border = FMath::IsFinite(BorderWidth) ? FMath::Max(0.f, BorderWidth) : 0.f;
	const FVector2D BorderVector(Border, Border);
	bool bDrew = false;
	for (const FCC_HealthBarDisplayItem& Item : Model->GetItems())
	{
		if (Item.MaxDistance > 0 && FVector::DistSquared(Projector.GetViewOrigin(), Item.WorldAnchor) > FMath::Square(Item.MaxDistance)) continue;
		// 背后或锚点落在画面外时跳过；这里不做场景遮挡检测，默认允许透墙显示。
		FVector2D PlayerLocal;
		if (!Projector.ProjectToPlayerLocal(Item.WorldAnchor, PlayerLocal, true)) continue;
		const FVector2D Center = Projector.PlayerLocalToWidget(PlayerLocal) + ScreenOffset;
		const FVector2D Size = Item.Size;
		const FVector2D Position = Center - Size * 0.5f;
		// 所有角色共用 Brush、裁剪状态及三个固定层级：边框 +1、背景 +2、填充 +3。
		// 不逐角色递增层级，为 Slate 合批创造条件；实际批次数仍由资源、裁剪和其他 UI 决定。
		if (Border > 0) FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 1,
			Geometry.ToPaintGeometry(Size + BorderVector * 2, FSlateLayoutTransform(Position - BorderVector)),
			Brush, ESlateDrawEffect::None, BorderColor * Tint);
		FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 2,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Brush, ESlateDrawEffect::None, BackgroundColor * Tint);
		if (Item.Fraction > 0) FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 3,
			Geometry.ToPaintGeometry(FVector2D(Size.X * Item.Fraction, Size.Y), FSlateLayoutTransform(Position)),
			Brush, ESlateDrawEffect::None, Item.Color * Tint);
		bDrew = true;
	}
	// 无血条绘制时不占额外层级；有绘制则报告使用过的最高层级。
	return bDrew ? BaseLayer + 3 : BaseLayer;
}
