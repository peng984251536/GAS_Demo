// 未配置自定义控件时使用的原生单条血条：边框 + 背景 + 填充，外观与原批量绘制版一致。
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HealthBar/CC_HealthBarItem.h"
#include "CC_DefaultHealthBarItemWidget.generated.h"

class UBorder;
class UImage;
class USizeBox;

/**
 * 默认单条血条。设计器为空时由 C++ 构建简易布局；也可以派生蓝图后自己做设计树，
 * 再覆盖 On Health Bar Updated 刷新自己的控件（此时原生布局不会创建）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_DefaultHealthBarItemWidget : public UUserWidget, public ICC_HealthBarItem
{
	GENERATED_BODY()
public:
	UCC_DefaultHealthBarItemWidget(const FObjectInitializer& ObjectInitializer);
	/** 背景颜色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor BackgroundColor = FLinearColor(0.03f, 0.03f, 0.03f, 0.85f);
	/** 边框颜色；填充颜色取角色配置中的 Color。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor BorderColor = FLinearColor::Black;
	/** 边框宽度，HUD 布局单位；0 表示无边框。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(ClampMin="0")) float BorderWidth = 1.f;

	virtual void OnHealthBarUpdated_Implementation(const FCC_HealthBarItemData& Data) override;
protected:
	/** 蓝图没有设计树时构建原生布局。 */
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BarBox;
	UPROPERTY(Transient) TObjectPtr<USizeBox> FillBox;
	UPROPERTY(Transient) TObjectPtr<UImage> Fill;
};
