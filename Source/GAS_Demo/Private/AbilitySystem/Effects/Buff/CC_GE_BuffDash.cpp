#include "AbilitySystem/Effects/Buff/CC_GE_BuffDash.h"
#include "AbilitySystem/Abilities/Mechanics/CC_Dash.h"
#include "AbilitySystem/Abilities/Mechanics/CC_SummonAlly.h"
#include "GameplayEffectComponents/AbilitiesGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffBehavior_SplitArrow.h"

/**
 * 能力层配置：15 秒内由 GAS 授予冲刺 GA；重复施加刷新时间，不重复授予多份能力。
 * 这里没有移动或输入逻辑，能力激活后由 UCC_Dash 自行执行。
 */
UCC_GE_BuffDash::UCC_GE_BuffDash()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(15.f));
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
    UCC_BuffUIData* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = NSLOCTEXT("CCBuff", "DashName", "临时冲刺");
    UI->Description =
        NSLOCTEXT("CCBuff", "DashDescription", "获得冲刺能力 15 秒；通过 GAS 通用入口激活，到期后撤回能力。");
    GEComponents.Add(UI);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
    UAbilitiesGameplayEffectComponent* Grants =
        CreateDefaultSubobject<UAbilitiesGameplayEffectComponent>(TEXT("GrantDash"));
    /**
     * 到期、驱散、抑制或死亡移除 GE 时，立即取消进行中的冲刺并撤回 GA。
     * 能力不会在授予时自动执行，输入只按能力标签查找它。
     */
    FGameplayAbilitySpecConfig Config;
    Config.Ability = UCC_Dash::StaticClass();
    Config.RemovalPolicy = EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately;
    Grants->AddGrantedAbilityConfig(Config);
    GEComponents.Add(Grants);
}

/**
 * 能力层配置：无限持续，GA 授予即在身边召唤，成功后移除本 GE；同时最多持有一个。召唤物不随 Buff 消失。
 */
UCC_GE_BuffSummon::UCC_GE_BuffSummon()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    UCC_BuffUIData* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = NSLOCTEXT("CCBuff", "SummonName", "召唤随从");
    UI->Description = NSLOCTEXT("CCBuff", "SummonDescription", "在身边召唤一个为你作战的随从。");
    GEComponents.Add(UI);
    GEComponents.Add(CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules")));
    UAbilitiesGameplayEffectComponent* Grants =
        CreateDefaultSubobject<UAbilitiesGameplayEffectComponent>(TEXT("GrantSummon"));
    FGameplayAbilitySpecConfig Config;
    Config.Ability = UCC_SummonAlly::StaticClass();
    Config.RemovalPolicy = EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately;
    Grants->AddGrantedAbilityConfig(Config);
    GEComponents.Add(Grants);
    // 召唤 Buff 不同步给召唤物，否则召唤物也会召唤。
    UAssetTagsGameplayEffectComponent* AssetTags =
        CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
    FInheritedTagContainer Tags;
    Tags.Added.AddTag(CCBuffTags::NotShared);
    AssetTags->SetAndApplyAssetTagChanges(Tags);
    GEComponents.Add(AssetTags);
}

/**
 * 规则层配置：同来源最多叠 3 层，不同来源各自保存独立 GE 和 Behavior 实例。
 * 持续时间和刷新由 GAS 管理，模板只声明额外箭数、散射角与单箭倍率。
 */
UCC_GE_BuffSplitArrow::UCC_GE_BuffSplitArrow()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(20.f));
    StackingType = EGameplayEffectStackingType::AggregateBySource;
    StackLimitCount = 3;
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
    UCC_BuffUIData* UI = CreateDefaultSubobject<UCC_BuffUIData>(TEXT("BuffUI"));
    UI->DisplayName = NSLOCTEXT("CCBuff", "SplitArrowName", "分裂箭");
    UI->Description = NSLOCTEXT("CCBuff", "SplitArrowDescription",
                                "20 秒内每层额外射出 2 支箭，散射 30 度，单箭伤害 60%，最多 3 层。");
    GEComponents.Add(UI);
    UCC_BuffRulesComponent* Rules = CreateDefaultSubobject<UCC_BuffRulesComponent>(TEXT("BuffRules"));
    /**
     * CDO 内嵌配置使用 Archetype 标志，让蓝图编译和重新加载保留规则模板。
     * 运行时 BuffComponent 再复制模板，配置对象本身不保存单次射击状态。
     */
    Rules->Behaviors.Add(
        NewObject<UCC_BuffBehavior_SplitArrow>(Rules, TEXT("SplitArrowTemplate"), RF_Public | RF_ArchetypeObject));
    GEComponents.Add(Rules);
}
