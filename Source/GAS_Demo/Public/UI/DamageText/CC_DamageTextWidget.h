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
 *   1. 打开你的玩家 HUD 蓝图（CC_PlayerHUDWidget 派生）。
 *   2. 往 Overlay 里拖一个 Damage Text Widget，放在最上层。
 *   3. 确保它的 Slot 不参与命中测试（Visibility = Not Hit-Testable）。
 *   4. 字号和上升高度可以在这里直接调。
 *   5. Overlay Slot 设置水平/垂直 Fill，或 Canvas Slot 全屏拉伸；Owning Player 必须是本地玩家。
 *   此控件不会自动加入 HUD，也不会挂载在每个受击角色上。
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

	/** 实际绘制用的 Slate 控件。 */
	TSharedPtr<class SCC_DamageTextLayer> DamageTextLayer;
};
