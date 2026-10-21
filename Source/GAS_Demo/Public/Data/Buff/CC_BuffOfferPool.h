#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/CC_BuffSelectionTypes.h"
#include "CC_BuffOfferPool.generated.h"

/**
 * 三选一的 Buff 池：在编辑器里列出 X 个候选 Buff，每次随机抽 OfferCount 个给玩家选。
 * 运行时只读，不记录玩家已经选过什么；抽取规则只有"不放回、同一 GE 一轮只出现一次"。
 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_BuffOfferPool : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 候选 Buff 列表；无效条目（空类、抽象类、等级非法、缺少 UCC_BuffUIData）抽取时跳过。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Offer")
    TArray<FCC_BuffOfferOption> Entries;

    /** 每轮给出几个选项；池中有效条目不足时全部给出。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|Offer", meta = (ClampMin = "1", ClampMax = "8"))
    int32 OfferCount = 3;

    /**
     * 不放回地随机抽取本轮选项，同一 GE 类只出现一次，顺序也随机。
     * @param Random 调用方持有的随机流；测试可传固定种子得到确定结果。
     * @return 抽到的选项，数量为 min(OfferCount, 有效且不重复的条目数)。
     */
    TArray<FCC_BuffOfferOption> RollOptions(FRandomStream& Random) const;

    /**
     * 判断一个选项能否进入抽取：GE 类存在且非抽象、等级为有限正数、带 UCC_BuffUIData。
     */
    static bool IsOptionUsable(const FCC_BuffOfferOption& Option);

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
