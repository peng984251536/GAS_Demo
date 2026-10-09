#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "CC_CombatMainWidget.generated.h"

class UCC_PlayerVitalsWidget;

/**
 * 战斗主界面（原 CC_PlayerHUDWidget）：放在 Game 层的战斗 UI 根控件，只负责布局和把模型分发给子控件。
 * GAS 绑定和角色切换由 CC_CombatMainController 管理，数据保存在 CC_CombatMainModel。
 *
 * 子控件：
 *   - PlayerVitals（UCC_PlayerVitalsWidget）：玩家血蓝条。蓝图中放一个同名控件即自动绑定。
 *   - 伤害飘字层、技能栏、Buff 等以后都作为子控件放在这里。
 * 没有蓝图设计树时生成最小原生布局（左上角血蓝条）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_CombatMainWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** 战斗主界面不处理返回、不参与焦点竞争，也不能通过普通关闭入口移除。 */
	UCC_CombatMainWidget(const FObjectInitializer& ObjectInitializer);
protected:
	/** 有蓝图设计树时保留蓝图布局，否则创建原生示例布局。 */
	virtual void NativeOnInitialized() override;
	/** 模型更新后把数据分发给子控件，并转发 OnVitalsChanged 给蓝图。 */
	virtual void NativeOnModelChanged() override;
	/**
	 * 蓝图展示事件：首次绑定和四项属性任意变化时触发。
	 * 保留原 PlayerHUD 的签名，已有蓝图无需改动；新蓝图建议直接使用 PlayerVitals 子控件。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Combat")
	void OnVitalsChanged(float Health, float MaxHealth, float Mana, float MaxMana);

	/** 玩家血蓝条子控件；蓝图中同名控件自动绑定，可为空。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat", meta=(BindWidgetOptional))
	TObjectPtr<UCC_PlayerVitalsWidget> PlayerVitals;
};
