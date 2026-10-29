#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "CC_CombatMainWidget.generated.h"

class UCC_PlayerVitalsWidget;
class UCommonButtonBase;
class UTextBlock;

/**
 * 战斗主界面（原 CC_PlayerHUDWidget）：放在 Game 层的战斗 UI 根控件，只负责布局和把模型分发给子控件。
 * GAS 绑定和角色切换由 CC_CombatMainController 管理，数据保存在 CC_CombatMainModel。
 *
 * 子控件：
 *   - PlayerVitalsWidget_PlayerVitals（UCC_PlayerVitalsWidget）：玩家血蓝条。蓝图中放一个同名控件即自动绑定。
 *   - CommonButtonBase_BuffSelection（CommonButton，可选）：三选一入口，C++ 自动绑定点击；始终可点，没有待选奖励时点击不打开页面。
 *   - Widget_BuffRedDot（任意控件，可选）：红点，有待选奖励时显示，否则折叠。
 *   - TextBlock_BuffRewardCount（TextBlock，可选）：待选次数，有奖励时显示数字。
 *   - 技能栏、Buff、目标信息等以后都作为子控件放在这里。
 *   头顶血条与伤害飘字属于根布局的世界覆盖层，不放在本界面。
 * 没有蓝图设计树时生成最小原生布局（左上角血蓝条）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_CombatMainWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** 战斗主界面不处理返回、不参与焦点竞争，也不能通过普通关闭入口移除。 */
	UCC_CombatMainWidget(const FObjectInitializer& ObjectInitializer);
	/** 打开三选一；CommonButtonBase_BuffSelection 点击时自动调用，自定义入口（如手柄按键）也可直接调用。 */
	UFUNCTION(BlueprintCallable, Category="UI|Combat")
	bool OpenBuffSelection();
protected:
	/** 有蓝图设计树时保留蓝图布局，否则创建原生示例布局。 */
	virtual void NativeOnInitialized() override;
	/** 模型更新后把数据分发给子控件，并转发 OnVitalsChanged 给蓝图。 */
	virtual void NativeOnModelChanged() override;
	/**
	 * 蓝图展示事件：首次绑定和四项属性任意变化时触发。
	 * 保留原 PlayerHUD 的签名，已有蓝图无需改动；新蓝图建议直接使用 PlayerVitalsWidget_PlayerVitals 子控件。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Combat")
	void OnVitalsChanged(float Health, float MaxHealth, float Mana, float MaxMana);
	/** 待选奖励次数变化时触发（含首次绑定），按钮与红点已由 C++ 更新，这里只做额外表现（动画等）。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Combat")
	void OnBuffRewardsChanged(int32 PendingCount);

	/** 玩家血蓝条子控件；蓝图中同名控件自动绑定；缺失或类型不匹配时蓝图编译报错。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat", meta=(BindWidget))
	TObjectPtr<UCC_PlayerVitalsWidget> PlayerVitalsWidget_PlayerVitals;

	/** 三选一入口按钮；可选，同名控件自动绑定点击。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat", meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CommonButtonBase_BuffSelection;

	/** 红点；可选，有待选奖励时显示。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Widget_BuffRedDot;

	/** 待选次数文本；可选。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TextBlock_BuffRewardCount;

private:
	void HandleBuffSelectionClicked();
	/** 按待选次数更新按钮、红点与次数文本。 */
	void ApplyBuffRewards(int32 PendingCount);
	int32 LastBuffRewards = -1;
};
