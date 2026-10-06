#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "CC_PlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** HUD 展示层：读取 PlayerHUDModel，GAS 绑定和角色切换由 PlayerHUDController 管理。 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_PlayerHUDWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** HUD 不处理返回、不参与焦点竞争，也不能通过普通关闭入口移除。 */
	UCC_PlayerHUDWidget(const FObjectInitializer& ObjectInitializer);
protected:
	/** 原生类创建简易血蓝条；有蓝图设计树时保留蓝图布局。 */
	virtual void NativeOnInitialized() override;
	/** 模型更新后刷新原生控件并转发 OnVitalsChanged 给蓝图。 */
	virtual void NativeOnModelChanged() override;
	/** 蓝图展示事件：首次绑定和四项属性任意变化时触发，用来刷新自己的控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|GAS")
	void OnVitalsChanged(float Health, float MaxHealth, float Mana, float MaxMana);

private:
	/** 原生示例生命条；自定义蓝图布局中可以为空。 */
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	/** 原生示例法力条。 */
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ManaBar;
	/** 原生示例数值文字。 */
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VitalsText;
};
