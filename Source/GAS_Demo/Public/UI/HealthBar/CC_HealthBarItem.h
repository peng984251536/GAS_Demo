// 单条头顶血条控件的约定：任何 UserWidget 蓝图实现本接口，就能被血条层放到敌人头顶并驱动刷新。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UI/HealthBar/CC_HealthBarTypes.h"
#include "CC_HealthBarItem.generated.h"

UINTERFACE(BlueprintType, MinimalAPI, meta=(DisplayName="CC Health Bar Item"))
class UCC_HealthBarItem : public UInterface
{
	GENERATED_BODY()
};

/**
 * 单条头顶血条。
 *
 * 用法：新建一个普通 UserWidget 蓝图，在 Class Settings → Interfaces 中添加 CC Health Bar Item，
 * 然后在角色配置（CC_CharacterConfig → Overhead Health Bar → Item Widget Class）里选它。
 *
 * 控件由血条层用对象池复用：同一个实例可能先后显示给不同敌人。
 * 所以每次 On Health Bar Assigned 都要把上一个敌人留下的状态（文字、动画、颜色）重置干净。
 *
 * 位置：血条层把控件的底边中点对齐到敌人头顶锚点，控件按自身期望尺寸显示（Canvas Auto Size）。
 * 不要在控件里自己设置位置；需要上下微调时改配置里的 World Offset，或调整控件内部留白。
 */
class GAS_DEMO_API ICC_HealthBarItem
{
	GENERATED_BODY()
public:
	/** 分配给某个角色时调用（包括从对象池复用）。可在这里读取名字、等级等不常变化的信息。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|Health Bar")
	void OnHealthBarAssigned(AActor* Target);
	/** 数据变化时调用；插值期间每帧都会调用，可直接设置进度条比例。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|Health Bar")
	void OnHealthBarUpdated(const FCC_HealthBarItemData& Data);
	/** 不再显示（角色移除、死亡隐藏、离开画面或超出距离）并回到对象池时调用；在这里停止动画、清理引用。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|Health Bar")
	void OnHealthBarReleased();
};
