#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/Combat/CC_CombatTypes.h"
#include "CC_PlayerVitalsWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 战斗主界面中的玩家血蓝条子控件。
 * 纯展示：不订阅 GAS、不查找角色，由 CC_CombatMainWidget 在模型变化时调用 SetVitals 推送数据。
 * 蓝图派生时可在设计器中放置名为 HealthBar / ManaBar / VitalsText 的控件自动绑定，
 * 或者实现 On Vitals Changed 自己刷新；没有设计树时生成最小原生示例布局。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_PlayerVitalsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 推送一份新快照并刷新显示。 */
	UFUNCTION(BlueprintCallable, Category="UI|Combat")
	void SetVitals(const FCC_PlayerVitals& InVitals);
	/** 最近一次推送的快照。 */
	UFUNCTION(BlueprintPure, Category="UI|Combat")
	FCC_PlayerVitals GetVitals() const { return Vitals; }
protected:
	/** 有蓝图设计树时保留蓝图布局，否则构建最小血蓝条示例。 */
	virtual void NativeOnInitialized() override;
	/** 蓝图展示事件：每次 SetVitals 后触发。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Combat")
	void OnVitalsChanged(const FCC_PlayerVitals& InVitals);
private:
	/** 把当前快照写到已绑定的原生控件上。 */
	void ApplyToWidgets() const;
	/** 最近一次快照。 */
	UPROPERTY(Transient) FCC_PlayerVitals Vitals;
	/** 生命条；蓝图中同名控件会自动绑定，可为空。 */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;
	/** 法力条；可为空。 */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ManaBar;
	/** 数值文字；可为空。 */
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> VitalsText;
};
