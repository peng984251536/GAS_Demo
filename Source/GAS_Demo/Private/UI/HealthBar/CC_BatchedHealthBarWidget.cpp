#include "UI/HealthBar/CC_BatchedHealthBarWidget.h"
#include "UI/HealthBar/CC_BatchedHealthBarSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "SceneView.h"
#include "Styling/CoreStyle.h"
#include "GAS_Demo.h"

UCC_BatchedHealthBarWidget::UCC_BatchedHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 全屏 UI 只负责显示，不阻挡鼠标点击或 HUD 按钮。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// 角色和相机移动不一定触发 UMG 属性变化；设为易变控件，避免失效缓存导致血条位置停住。
	ForceVolatile(true);
}

void UCC_BatchedHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UCC_BatchedHealthBarSubsystem* Manager = UCC_BatchedHealthBarSubsystem::GetHealthBarManager(GetOwningPlayer()))
	{
		RegisteredManager = Manager;
		if (Manager->AddDrawLayer() > 1)
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("同一本地玩家存在多个头顶血条绘制层（%s），血条会被重复绘制。")
				TEXT("根布局已自动创建血条层，请删除 PlayerController 等处手动 Add to Player Screen 的血条控件。"), *GetPathName());
		}
	}
}

void UCC_BatchedHealthBarWidget::NativeDestruct()
{
	if (UCC_BatchedHealthBarSubsystem* Manager = RegisteredManager.Get()) Manager->RemoveDrawLayer();
	RegisteredManager.Reset();
	Super::NativeDestruct();
}

int32 UCC_BatchedHealthBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& DrawElements, int32 LayerId,
	const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	// 保留父类及蓝图已有内容的最高层级，血条从其上方继续绘制。
	const int32 BaseLayer = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, WidgetStyle, bParentEnabled);
	APlayerController* PC = GetOwningPlayer();
	const UCC_BatchedHealthBarSubsystem* Manager = UCC_BatchedHealthBarSubsystem::GetHealthBarManager(PC);
	if (!Manager || Manager->GetEntries().IsEmpty()) return BaseLayer;
	ULocalPlayer* Player = PC->GetLocalPlayer();
	if (!Player->ViewportClient || !Player->ViewportClient->Viewport) return BaseLayer;
	// 整批血条共用相机投影矩阵；不为每个角色单独构建投影数据。
	FSceneViewProjectionData Projection;
	if (!Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)) return BaseLayer;
	// PlayerRect 为该玩家视口范围；ViewRect 为保持宽高比后扣除黑边的实际画面范围。
	const FIntRect PlayerRect = Projection.GetViewRect();
	const FIntRect ViewRect = Projection.GetConstrainedViewRect();
	if (PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0) return BaseLayer;
	const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
	const FGeometry PlayerGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PC);
	const FVector2D PixelToLocal = PlayerGeometry.GetLocalSize() / FVector2D(PlayerRect.Width(), PlayerRect.Height());
	// 共用白色纹理资源，通过顶点颜色控制样式，避免逐角色材质打断合批。
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FLinearColor Tint = WidgetStyle.GetColorAndOpacityTint();
	const FVector CameraLocation = Projection.ViewOrigin;
	const float Border = FMath::IsFinite(BorderWidth) ? FMath::Max(0.f, BorderWidth) : 0.f;
	bool bDrew = false;
	for (const FCC_HealthBarEntry& Entry : Manager->GetEntries())
	{
		// 先按数值、显隐与距离过滤，减少不需要的世界坐标投影。
		const AActor* Actor = Entry.Actor.Get();
		if (!Actor || !Entry.bVisible || !Entry.bReady || Actor->IsHidden() || Entry.MaxHealth <= 0) continue;
		if (Entry.Options.bHideWhenDead && Entry.Health <= 0) continue;
		if (Entry.Options.bHideWhenFull && Entry.Health >= Entry.MaxHealth) continue;
		const FVector Anchor = Actor->GetActorLocation() + Entry.Options.WorldOffset;
		if (Entry.Options.MaxDistance > 0 && FVector::DistSquared(CameraLocation, Anchor) > FMath::Square(Entry.Options.MaxDistance)) continue;
		// 背后或锚点落在画面外时跳过；这里不做场景遮挡检测，默认允许透墙显示。
		FVector2D Pixel;
		if (!FSceneView::ProjectWorldToScreen(Anchor, ViewRect, ViewProjection, Pixel)) continue;
		if (Pixel.X < ViewRect.Min.X || Pixel.X >= ViewRect.Max.X || Pixel.Y < ViewRect.Min.Y || Pixel.Y >= ViewRect.Max.Y) continue;
		// 投影结果是视口像素。先减去分屏区域原点，再缩放为本地玩家 HUD 布局坐标，
		// 经玩家几何转换到 Slate 绝对坐标，最后转入当前控件；处理窗口偏移、DPI、分屏和黑边。
		const FVector2D PlayerLocal = (Pixel - FVector2D(PlayerRect.Min.X, PlayerRect.Min.Y)) * PixelToLocal;
		const FVector2D Center = Geometry.AbsoluteToLocal(PlayerGeometry.LocalToAbsolute(PlayerLocal)) + ScreenOffset;
		const FVector2D Size = Entry.Options.Size;
		const FVector2D Position = Center - Size * 0.5f;
		const FVector2D BorderVector(Border, Border);
		// 所有角色共用 Brush、裁剪状态及三个固定层级：边框 +1、背景 +2、填充 +3。
		// 不逐角色递增层级，为 Slate 合批创造条件；实际批次数仍由资源、裁剪和其他 UI 决定。
		if (Border > 0) FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 1,
			Geometry.ToPaintGeometry(Size + BorderVector * 2, FSlateLayoutTransform(Position - BorderVector)),
			Brush, ESlateDrawEffect::None, BorderColor * Tint);
		FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 2,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Brush, ESlateDrawEffect::None, BackgroundColor * Tint);
		const float Fraction = FMath::Clamp(Entry.DisplayedFraction, 0.f, 1.f);
		if (Fraction > 0) FSlateDrawElement::MakeBox(DrawElements, BaseLayer + 3,
			Geometry.ToPaintGeometry(FVector2D(Size.X * Fraction, Size.Y), FSlateLayoutTransform(Position)),
			Brush, ESlateDrawEffect::None, Entry.Options.Color * Tint);
		bDrew = true;
	}
	// 无血条绘制时不占额外层级；有绘制则报告使用过的最高层级。
	return bDrew ? BaseLayer + 3 : BaseLayer;
}
