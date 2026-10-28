#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Templates/SubclassOf.h"
#include "CC_BuffSelectionTypes.generated.h"

/**
 * 三选一中的一个候选项：施加哪个 Buff GE、用什么等级。
 * 同时用作池配置条目和复制给客户端的选项；显示信息在客户端从 GE 的 UCC_BuffUIData 读取。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_BuffOfferOption
{
    GENERATED_BODY()

    /** Buff 的 GE 类，必须带 UCC_BuffUIData，否则卡片没有可显示的名称和图标。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff|Offer")
    TSubclassOf<UGameplayEffect> Effect;

    /** 施加时使用的 GE 等级，必须是有限正数。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff|Offer", meta = (ClampMin = "0.01"))
    float Level = 1.f;

    bool operator==(const FCC_BuffOfferOption& Other) const
    {
        return Effect == Other.Effect && Level == Other.Level;
    }
};

/**
 * 一次三选一：服务器分配的编号和本次抽到的选项。
 * 编号用于校验客户端的选择是否针对当前这一轮，防止迟到或重复的请求选中下一轮的卡片。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_BuffOffer
{
    GENERATED_BODY()

    /** 服务器递增分配，0 表示当前没有待选的一轮。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Offer")
    int32 OfferId = 0;

    /** 当前一轮与排队轮次的总数，由服务器维护并随选项复制给拥有者。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Offer")
    int32 PendingCount = 0;

    /** 本轮候选项，下标就是选择请求里的 OptionIndex。 */
    UPROPERTY(BlueprintReadOnly, Category = "Buff|Offer")
    TArray<FCC_BuffOfferOption> Options;

    /** 是否有一轮正在等待玩家选择。 */
    bool IsValid() const { return OfferId > 0 && Options.Num() > 0; }
};

/** 服务器处理一次选择请求的结果。 */
UENUM(BlueprintType)
enum class ECC_BuffChoiceResult : uint8
{
    /** 还没有结果，界面用来表示"无错误提示"。 */
    None,
    /** 已施加，本轮结束。 */
    Accepted,
    /** 请求针对的不是当前这一轮（已选过、已被清空或已进入下一轮）。 */
    StaleOffer,
    /** 选项下标越界。 */
    InvalidIndex,
    /** 当前没有可施加的存活角色，本轮保留，玩家可稍后再选。 */
    NoTarget,
    /** GAS 拒绝施加（免疫、叠层已满等），本轮保留，玩家可改选其他卡片。 */
    ApplyFailed
};
