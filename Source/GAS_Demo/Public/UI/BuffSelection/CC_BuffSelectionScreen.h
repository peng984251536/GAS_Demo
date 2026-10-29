#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/BuffSelection/CC_BuffSelectionViewTypes.h"
#include "CC_BuffSelectionScreen.generated.h"

class UPanelWidget;
class UCC_BuffOfferCardWidget;
class UCC_BuffSelectionModel;

/**
 * 三选一页面基类。蓝图继承它搭布局：
 * - 放一个名为 PanelWidget_Cards 的面板（HorizontalBox 等），并在类默认值里设置 CardClass，卡片会自动生成；
 * - 或者不放容器，自己在 On Buff Offer Changed 里按 Cards 更新控件，再调用 Choose Option。
 * - 可选放一个名为 CommonButtonBase_Close 的 CommonButton（基类 CC_ActivatableWidget 提供），点击关闭页面。
 * 关闭页面不消耗奖励（下次点击入口继续选）；当前一轮选完且没有下一轮时自动关闭，有下一轮时原地换成新卡片。
 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_BuffSelectionScreen : public UCC_ActivatableWidget
{
    GENERATED_BODY()

public:
    UCC_BuffSelectionScreen(const FObjectInitializer& ObjectInitializer);

    /**
     * 选择一张卡片；卡片按钮会自动调用，自定义布局时由蓝图调用。
     * @return 请求已发出时返回 true。
     */
    UFUNCTION(BlueprintCallable, Category = "UI|BuffSelection")
    bool ChooseOption(int32 OptionIndex);

    /** 本页的展示模型；页面未激活时可能为空。 */
    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    UCC_BuffSelectionModel* GetSelectionModel() const;

protected:
    virtual void NativeOnModelChanged() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    /**
     * 卡片、提交状态或失败原因变化时调用；自动生成卡片之后触发。
     * @param Cards 当前一轮的卡片数据，空数组表示即将关闭。
     * @param bSubmitting 正在等待服务器结果，可显示加载状态。
     * @param LastResult 本轮最近一次被拒绝的原因，None 表示无需提示。
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|BuffSelection")
    void OnBuffOfferChanged(const TArray<FCC_BuffOfferCardData>& Cards, bool bSubmitting,
                            ECC_BuffChoiceResult LastResult);

    /** 可选：自动生成卡片的容器，名称必须是 PanelWidget_Cards。 */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "UI|BuffSelection")
    TObjectPtr<UPanelWidget> PanelWidget_Cards;

    /** 自动生成时使用的卡片蓝图类。 */
    UPROPERTY(EditDefaultsOnly, Category = "UI|BuffSelection")
    TSubclassOf<UCC_BuffOfferCardWidget> CardClass;

private:
    /** 一轮换了才重建卡片；只有提交状态变化时只切换可点击状态。 */
    void RebuildCards(const UCC_BuffSelectionModel& Model);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCC_BuffOfferCardWidget>> SpawnedCards;
    int32 BuiltOfferId = 0;
    /** 没有选项后等待关闭；过渡中关闭会被拒绝，因此每帧重试（暂停时控件仍会 Tick）。 */
    bool bCloseRequested = false;
};
