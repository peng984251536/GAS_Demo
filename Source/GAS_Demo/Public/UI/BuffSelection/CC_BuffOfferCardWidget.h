#pragma once

#include "UI/Framework/CC_ButtonBase.h"
#include "UI/BuffSelection/CC_BuffSelectionViewTypes.h"
#include "CC_BuffOfferCardWidget.generated.h"

class UImage;
class UTextBlock;
class UCC_BuffSelectionScreen;

/**
 * 三选一的单张卡片，本身就是 CommonButton，支持鼠标、键盘和手柄焦点。
 * 设计器里必须放名为 TextBlock_Name、TextBlock_Description、Image_Icon 的控件自动填充；
 * 缺少同名、兼容类型的控件时，Widget Blueprint 编译报错。没有图标数据时会隐藏 Image_Icon。
 * 其他表现（稀有度边框、动画等）在 On Card Data Set 里自己处理。点击后交给所属页面选择。
 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_BuffOfferCardWidget : public UCC_ButtonBase
{
    GENERATED_BODY()

public:
    /** 选卡属于确认操作，点击音效使用 UI.Confirm。 */
    UCC_BuffOfferCardWidget(const FObjectInitializer& ObjectInitializer);

    /** 页面生成卡片后调用，写入数据并记录所属页面。 */
    void SetCardData(const FCC_BuffOfferCardData& InData, UCC_BuffSelectionScreen* InOwner);

    UFUNCTION(BlueprintPure, Category = "UI|BuffSelection")
    FCC_BuffOfferCardData GetCardData() const { return Data; }

protected:
    virtual void NativeOnClicked() override;

    /** 数据写入后调用，在这里更新自定义表现。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|BuffSelection")
    void OnCardDataSet(const FCC_BuffOfferCardData& CardData);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|BuffSelection")
    TObjectPtr<UTextBlock> TextBlock_Name;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|BuffSelection")
    TObjectPtr<UTextBlock> TextBlock_Description;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|BuffSelection")
    TObjectPtr<UImage> Image_Icon;

private:
    UPROPERTY(Transient)
    FCC_BuffOfferCardData Data;

    /** 页面持有卡片，卡片只弱引用页面。 */
    TWeakObjectPtr<UCC_BuffSelectionScreen> OwnerScreen;
};
