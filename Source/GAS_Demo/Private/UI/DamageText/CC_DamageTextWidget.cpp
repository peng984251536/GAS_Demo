// 只管理控制器会话与 Slate 层的创建、属性读取和释放；不持有伤害条目，也不访问子系统。
#include "UI/DamageText/CC_DamageTextWidget.h"

#include "UI/DamageText/SCC_DamageTextLayer.h"
#include "UI/DamageText/CC_DamageTextController.h"

// 设置不参与命中测试，保证飘字画布不会拦截 HUD 上的鼠标输入。
UCC_DamageTextWidget::UCC_DamageTextWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 飘字绝不能参与命中测试，否则会挡住 HUD 上的所有点击。
	// 在构造里设一次，蓝图里也不必再改。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ControllerClass = UCC_DamageTextController::StaticClass();
}

// 释放 Slate 引用并结束控制器会话，供控件结束、切图或重建时使用。
void UCC_DamageTextWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	if (OverlayController) OverlayController->Release();
	OverlayController = nullptr;

	// 必须置空，否则 Slate 重建（例如关卡切换、DPI 变化）后会持有已销毁的控件。
	DamageTextLayer.Reset();
}

// 先开始控制器会话，再创建共享绘制层：绑定字号和上升高度，传入模型与用于投影的 Owning Player。
TSharedRef<SWidget> UCC_DamageTextWidget::RebuildWidget()
{
	// 设计器预览没有本地玩家，不创建控制器；绘制层拿到空模型时什么也不画。
	if (!IsDesignTime() && !OverlayController)
	{
		OverlayController = UCC_UIController::CreateForView(this, ControllerClass);
		if (OverlayController) OverlayController->Activate();
	}
	UCC_DamageTextModel* Model = OverlayController ? Cast<UCC_DamageTextModel>(OverlayController->GetModel()) : nullptr;

	// 用 TAttribute 惰性绑定，编辑器里改字号/高度会立即生效，
	// 不需要在 SynchronizeProperties 里重建 Slate 控件。
	DamageTextLayer = SNew(SCC_DamageTextLayer)
		.FontSize(TAttribute<float>::Create(
			TAttribute<float>::FGetter::CreateUObject(this, &UCC_DamageTextWidget::GetFontSizeValue)))
		.CritFontSize(TAttribute<float>::Create(
			TAttribute<float>::FGetter::CreateUObject(this, &UCC_DamageTextWidget::GetCriticalFontSizeValue)))
		.RiseHeight(TAttribute<float>::Create(
			TAttribute<float>::FGetter::CreateUObject(this, &UCC_DamageTextWidget::GetRiseHeightValue)))
		.PlayerController(GetOwningPlayer())
		.Model(Model);

	return DamageTextLayer.ToSharedRef();
}

// 属性同步时只使绘制失效；样式值由 getter 读取，不重复构造 Slate 控件。
void UCC_DamageTextWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 属性通过 TAttribute 惰性读取，这里只需让 Slate 重绘一次。
	if (DamageTextLayer.IsValid())
	{
		DamageTextLayer->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

// 提供普通字号的惰性读取接口。
float UCC_DamageTextWidget::GetFontSizeValue() const
{
	return FontSize;
}

// 提供暴击字号的惰性读取接口。
float UCC_DamageTextWidget::GetCriticalFontSizeValue() const
{
	return CriticalFontSize;
}

// 提供一个生命周期内的总上升高度。
float UCC_DamageTextWidget::GetRiseHeightValue() const
{
	return RiseHeight;
}

#if WITH_EDITOR
// 在 UMG 设计器控件列表中归入 UI 分类，仅编辑器构建需要。
const FText UCC_DamageTextWidget::GetPaletteCategory()
{
	return FText::FromString(TEXT("UI"));
}
#endif
