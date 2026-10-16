#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "GameplayTagContainer.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "UI/WorldIndicator/CC_WorldIndicatorWidget.h"
#include "CC_DamageNumberWidget.generated.h"

class UTextBlock;
class UCC_DamageNumberIndicator;

/**
 * 伤害 / 治疗数字控件（世界指示器控件）。
 *
 * 默认：C++ 驱动上飘、淡入淡出和暴击缩放，Lifetime 秒后自己移除。设计树为空时创建一个 TextBlock；
 * 蓝图设计树里名为 DamageText 的 TextBlock 会自动收到文字、字号和颜色。
 *
 * 想用 UMG 动画：派生蓝图，关闭 Drive Animation In Code，在 On Damage Number Shown 里播放动画，
 * 动画结束时调用 Finish Damage Number。动画只作用在内部控件上，不要改根控件位置（由指示器层负责）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_DamageNumberWidget : public UUserWidget, public ICC_WorldIndicatorWidget
{
	GENERATED_BODY()
public:
	UCC_DamageNumberWidget(const FObjectInitializer& ObjectInitializer);

	/** 所有数字共用一种字体（只改字号），利于合批。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style") FSlateFontInfo Font;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style", meta=(ClampMin="1")) int32 FontSize = 28;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style", meta=(ClampMin="1")) int32 CriticalFontSize = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style") FLinearColor DamageColor = FLinearColor(1.f, 0.12f, 0.12f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style") FLinearColor CriticalColor = FLinearColor(1.f, 0.55f, 0.1f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Style") FLinearColor HealingColor = FLinearColor(0.15f, 0.5f, 1.f);

	/** true：C++ 驱动下面的动画并按 Lifetime 自动结束；false：由蓝图动画驱动并调用 Finish Damage Number。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Animation") bool bDriveAnimationInCode = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Animation", meta=(ClampMin="0.1")) float Lifetime = 1.1f;
	/** 整个生命周期内上升的高度（HUD 布局单位）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Number|Animation", meta=(ClampMin="0")) float RiseHeight = 80.f;

	/** 结束显示：移除指示器，控件回到对象池。蓝图动画结束时调用。 */
	UFUNCTION(BlueprintCallable, Category="Damage Number")
	void FinishDamageNumber();

	virtual void BindIndicator_Implementation(UCC_WorldIndicator* Indicator) override;
	virtual void UnbindIndicator_Implementation() override;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** 数字开始显示（含对象池复用）。可在这里播放 UMG 动画、设置图标。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Damage Number")
	void OnDamageNumberShown(float Amount, ECC_DamageTextStyle Style, FGameplayTag DamageType);

	/** 蓝图里名为 DamageText 的 TextBlock 自动绑定。 */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DamageText;
private:
	/** C++ 动画作用在设计树根控件上（不碰本控件的渲染平移）。 */
	void ApplyCodeAnimation(float T);

	UPROPERTY(Transient) TObjectPtr<UCC_DamageNumberIndicator> BoundNumber;
	float Elapsed = 0.f;
	bool bCritical = false;
};
