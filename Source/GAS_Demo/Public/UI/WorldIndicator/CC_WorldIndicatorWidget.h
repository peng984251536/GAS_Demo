// 世界指示器控件的约定：任何 UserWidget 蓝图实现本接口即可被指示器层显示。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CC_WorldIndicatorWidget.generated.h"

class UCC_WorldIndicator;

UINTERFACE(BlueprintType, MinimalAPI, meta=(DisplayName="CC World Indicator Widget"))
class UCC_WorldIndicatorWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 指示器控件。位置由指示器层负责（每帧 SetRenderTranslation），控件不要自己改根控件的位置或渲染平移；
 * 需要上飘、抖动等动画时，动画作用在内部子控件上。
 *
 * 控件由对象池复用：同一个实例可能先后绑定到不同指示器，BindIndicator 里要把上一次的状态重置干净，
 * UnbindIndicator 里要取消所有订阅、停止动画。
 */
class GAS_DEMO_API ICC_WorldIndicatorWidget
{
	GENERATED_BODY()
public:
	/** 开始显示某个指示器（包括从对象池复用）。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|World Indicator")
	void BindIndicator(UCC_WorldIndicator* Indicator);
	/** 指示器被移除，控件即将回到对象池。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|World Indicator")
	void UnbindIndicator();
};
