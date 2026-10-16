#pragma once
#include "BuffSelection/CC_BuffSelectionComponent.h"
#include "CC_BuffSelectionTestTypes.generated.h"

/**
 * 自动化测试用的选择组件：可替换施加结果，并公开服务器校验入口。
 * 不出现在添加组件的下拉列表中，不用于正式玩法。
 */
UCLASS(NotBlueprintable, HideDropdown)
class UCC_BuffSelectionTestComponent : public UCC_BuffSelectionComponent
{
    GENERATED_BODY()
public:
    /** true 时走正式的施加逻辑（读取 Pawn 的 BuffComponent）。 */
    bool bUseDefaultApply = false;
    /** 模拟的施加结果。 */
    ECC_BuffChoiceResult NextResult = ECC_BuffChoiceResult::Accepted;
    /** 模拟施加成功的选项记录。 */
    TArray<FCC_BuffOfferOption> Applied;

    ECC_BuffChoiceResult Resolve(int32 OfferId, int32 OptionIndex) { return ResolveChoice(OfferId, OptionIndex); }

protected:
    virtual ECC_BuffChoiceResult ApplyOption(const FCC_BuffOfferOption& Option) override
    {
        if (bUseDefaultApply)
        {
            return Super::ApplyOption(Option);
        }
        if (NextResult == ECC_BuffChoiceResult::Accepted)
        {
            Applied.Add(Option);
        }
        return NextResult;
    }
};
