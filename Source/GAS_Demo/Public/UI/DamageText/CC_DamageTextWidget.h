// 供 UMG 设计器使用的包装控件：在玩家 HUD 放置一次并铺满绘制区域，无需挂在角色上。
#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CC_DamageTextWidget.generated.h"

/**
 * 伤害飘字绘制层的 UMG 包装。
 *
 * 作用：让 SCC_DamageTextLayer 能像普通控件一样拖进 UMG 设计器（放进 HUD 的 Overlay），
 * 而不需要在 C++ 里手动构造 Slate 并 AddToViewport。
 *
 * 使用方式：
 *   默认由 CC_RootLayout 的世界覆盖层自动创建（见 DamageTextLayerClass），不需要再往 HUD 蓝图里拖。
 *   若 HUD 蓝图中仍有旧的 Damage Text Widget，请删除，否则飘字会画两遍（运行时会输出警告）。
 *   不经过根布局、单独使用时：放进 Overlay 并全屏拉伸，保持 Not Hit-Testable，Owning Player 必须是本地玩家。
 *
 * 注意：这是普通 UWidget，不是 CommonUI 的 ActivatableWidget。
 * 它不需要输入路由，也不该抢焦点——用 ActivatableWidget 反而会引入多余的焦点副作用。
 */
UCLASS(meta = (DisplayName = "Damage Text Widget"))
class GAS_DEMO_API UCC_DamageTextWidget : public UWidget
{
	GENERATED_BODY()

public:
	UCC_DamageTextWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	// --- UWidget ---
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	/** 普通伤害字号。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Text", meta = (ClampMin = "1.0"))
	float FontSize = 28.0f;

	/** 暴击字号。应当明显大于普通字号才能形成视觉层级。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Text", meta = (ClampMin = "1.0"))
	float CriticalFontSize = 40.0f;

	/** 整个生命周期内向上飘升的像素高度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Text", meta = (ClampMin = "0.0"))
	float RiseHeight = 80.0f;

private:
	/** 供 Slate TAttribute 惰性读取的属性访问器。 */
	float GetFontSizeValue() const;
	float GetCriticalFontSizeValue() const;
	float GetRiseHeightValue() const;

	/** 构建时登记的玩家，释放时撤销同一份登记。 */
	TWeakObjectPtr<APlayerController> RegisteredPlayer;
	/** 实际绘制用的 Slate 控件。 */
	TSharedPtr<class SCC_DamageTextLayer> DamageTextLayer;
};
