#pragma once

#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/BuffSelection/CC_BuffSelectionViewTypes.h"
#include "CC_BuffSelectionController.generated.h"

class UCC_BuffSelectionComponent;

/**
 * 三选一界面的展示模型：当前一轮的卡片、是否正在提交、最近一次失败原因。
 * 只保存快照，不持有控件，也不直接访问选择组件；写入口只对 Controller 开放。
 */
UCLASS()
class GAS_DEMO_API UCC_BuffSelectionModel : public UCC_UIModel
{
    GENERATED_BODY()

public:
    /** 当前一轮编号，0 表示没有待选内容（界面应关闭）。 */
    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    int32 GetOfferId() const { return OfferId; }

    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    bool HasOffer() const { return OfferId > 0 && !Cards.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    TArray<FCC_BuffOfferCardData> GetCards() const { return Cards; }

    /** 已提交、等待服务器结果；此时卡片应不可点击。 */
    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    bool IsSubmitting() const { return bSubmitting; }

    /** 本轮最近一次被拒绝的原因；None 表示无需提示。换到新一轮时重置。 */
    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    ECC_BuffChoiceResult GetLastResult() const { return LastResult; }

private:
    /** 一次写入完整状态，有变化才广播。 */
    void SetState(int32 InOfferId, TArray<FCC_BuffOfferCardData>&& InCards, bool bInSubmitting,
                  ECC_BuffChoiceResult InLastResult);

    UPROPERTY(Transient) int32 OfferId = 0;
    UPROPERTY(Transient) TArray<FCC_BuffOfferCardData> Cards;
    UPROPERTY(Transient) bool bSubmitting = false;
    UPROPERTY(Transient) ECC_BuffChoiceResult LastResult = ECC_BuffChoiceResult::None;

    friend class UCC_BuffSelectionController;
};

/**
 * 三选一界面控制器：监听本地玩家的 UCC_BuffSelectionComponent，把当前一轮转成卡片快照，
 * 并把玩家的点击转成选择请求。不做页面导航，界面在没有选项时自行关闭。
 */
UCLASS()
class GAS_DEMO_API UCC_BuffSelectionController : public UCC_UIController
{
    GENERATED_BODY()

public:
    UCC_BuffSelectionController();

    /**
     * 选择当前一轮的某张卡片；页面不是顶层、正在过渡或已在提交中时拒绝。
     * @param OptionIndex 卡片的 OptionIndex。
     * @return 请求已发给服务器时返回 true；结果稍后体现在模型上。
     */
    UFUNCTION(BlueprintCallable, Category = "UI|BuffSelection")
    bool ChooseOption(int32 OptionIndex);

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

private:
    UFUNCTION() void HandleOfferChanged();
    UFUNCTION() void HandleChoiceResolved(int32 OfferId, ECC_BuffChoiceResult Result);
    /** 读取组件状态写入模型。 */
    void Refresh();

    /** 选择组件属于 PlayerController，控制器只弱引用。 */
    TWeakObjectPtr<UCC_BuffSelectionComponent> Selection;
    /** 当前一轮最近一次失败原因；换轮时清空。 */
    ECC_BuffChoiceResult LastResult = ECC_BuffChoiceResult::None;
    int32 LastResultOfferId = 0;
};
