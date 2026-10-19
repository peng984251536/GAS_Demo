#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

bool FCC_WorldOverlayProjector::Initialize(APlayerController* PlayerController, const FGeometry& InWidgetGeometry)
{
	ULocalPlayer* Player = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!Player || !Player->ViewportClient || !Player->ViewportClient->Viewport) return false;
	FSceneViewProjectionData Projection;
	if (!Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)) return false;
	ViewRect = Projection.GetConstrainedViewRect();
	PlayerRect = Projection.GetViewRect();
	if (ViewRect.Width() <= 0 || ViewRect.Height() <= 0 || PlayerRect.Width() <= 0 || PlayerRect.Height() <= 0) return false;
	// ComputeViewProjectionMatrix 已包含 -ViewOrigin 平移，投影时直接传世界坐标。
	ViewProjection = Projection.ComputeViewProjectionMatrix();
	ViewOrigin = Projection.ViewOrigin;
	PlayerGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController);
	WidgetGeometry = InWidgetGeometry;
	PixelToLocal = PlayerGeometry.GetLocalSize() / FVector2D(PlayerRect.Width(), PlayerRect.Height());
	return true;
}

bool FCC_WorldOverlayProjector::ProjectToPlayerLocal(const FVector& WorldLocation, FVector2D& OutPlayerLocal, bool bRequireInsideView) const
{
	FVector2D Pixel;
	if (!FSceneView::ProjectWorldToScreen(WorldLocation, ViewRect, ViewProjection, Pixel)) return false;
	if (bRequireInsideView && (Pixel.X < ViewRect.Min.X || Pixel.X >= ViewRect.Max.X || Pixel.Y < ViewRect.Min.Y || Pixel.Y >= ViewRect.Max.Y)) return false;
	// 投影结果是视口像素：先减去分屏区域原点，再缩放为本地玩家 HUD 布局单位。
	OutPlayerLocal = (Pixel - FVector2D(PlayerRect.Min.X, PlayerRect.Min.Y)) * PixelToLocal;
	return true;
}

FVector2D FCC_WorldOverlayProjector::PlayerLocalToWidget(const FVector2D& PlayerLocal) const
{
	return WidgetGeometry.AbsoluteToLocal(PlayerGeometry.LocalToAbsolute(PlayerLocal));
}
