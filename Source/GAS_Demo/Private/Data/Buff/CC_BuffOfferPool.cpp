#include "Data/Buff/CC_BuffOfferPool.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

bool UCC_BuffOfferPool::IsOptionUsable(const FCC_BuffOfferOption& Option)
{
    if (!Option.Effect || Option.Effect->HasAnyClassFlags(CLASS_Abstract) || !FMath::IsFinite(Option.Level) ||
        Option.Level <= 0.f)
    {
        return false;
    }
    const UGameplayEffect* Definition = Option.Effect.GetDefaultObject();
    return Definition && Definition->FindComponent<UCC_BuffUIData>() != nullptr;
}

TArray<FCC_BuffOfferOption> UCC_BuffOfferPool::RollOptions(FRandomStream& Random) const
{
    // 先过滤出有效且 GE 类不重复的条目，重复配置只保留第一条。
    TArray<FCC_BuffOfferOption> Candidates;
    for (const FCC_BuffOfferOption& Entry : Entries)
    {
        if (IsOptionUsable(Entry) &&
            !Candidates.ContainsByPredicate([&Entry](const FCC_BuffOfferOption& Existing)
                                            { return Existing.Effect == Entry.Effect; }))
        {
            Candidates.Add(Entry);
        }
    }

    // 部分 Fisher-Yates：只打乱需要的前 Count 个位置，结果即不放回抽取。
    const int32 Count = FMath::Min(FMath::Max(1, OfferCount), Candidates.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const int32 Swap = Random.RandRange(Index, Candidates.Num() - 1);
        Candidates.Swap(Index, Swap);
    }
    Candidates.SetNum(Count);
    return Candidates;
}

#if WITH_EDITOR
EDataValidationResult UCC_BuffOfferPool::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    int32 Usable = 0;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        if (IsOptionUsable(Entries[Index]))
        {
            ++Usable;
            continue;
        }
        Context.AddWarning(FText::Format(
            NSLOCTEXT("BuffSelection", "InvalidEntry",
                      "Entries[{0}] 无法抽取：需要非抽象的 GE 类、正数等级，且 GE 带 CC Buff UI Data 组件。"),
            FText::AsNumber(Index)));
    }
    if (Usable < OfferCount)
    {
        Context.AddWarning(FText::Format(
            NSLOCTEXT("BuffSelection", "NotEnoughEntries", "有效条目 {0} 个，少于每轮数量 {1}，每轮会全部给出。"),
            FText::AsNumber(Usable), FText::AsNumber(OfferCount)));
    }
    return Result;
}
#endif
