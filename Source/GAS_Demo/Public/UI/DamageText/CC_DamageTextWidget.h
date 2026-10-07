// 伤害飘字绘制层的 UMG 包装：由 CC_RootLayout 的世界覆盖层创建并铺满视口，不挂在角色上，也不属于玩家 HUD。
#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CC_DamageTextWidget.generated.h"

class UCC_UIController;
class UCC_DamageTextController;

/**
 * 伤害飘字绘制层的 UMG 包装（MVC 中的 View）。
 *
 * 数据流：飘字子系统 → CC_DamageTextController → CC_DamageTextModel → SCC_DamageTextLayer。
 * 本控件持有控制器，把模型交给 Slate 绘制层；它和绘制层都不直接访问子系统。
 * 与 CC_PlayerHUDWidget 是两个独立视图：HUD 是入栈页面，飘字是根布局世界覆盖层里的常驻控件。
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

	/** 控制器类，决定数据来源；一般无需修改。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Architecture")
	TSubclassOf<UCC_DamageTextController> ControllerClass;

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

	/** Slate 存在期间持有的控制器；ReleaseSlateResources 时释放。 */
	UPROPERTY(Transient)
	TObjectPtr<UCC_UIController> OverlayController;
	/** 实际绘制用的 Slate 控件。 */
	TSharedPtr<class SCC_DamageTextLayer> DamageTextLayer;
};
