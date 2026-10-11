#include "GameplayCues/CC_GameplayCueManager.h"
#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "GameplayCueSet.h"
/**
 * 分发前恢复原生数字 Cue 映射和默认对象，再交给 GAS 正常路由。
 * @param Target 接收当前 Cue 的目标 Actor。
 * @param Tag 当前要路由的 GameplayCue 标签。
 * @param Event 本次 GameplayCue 事件类型。
 * @param Params 需要分发的 GameplayCue 表现参数。
 * @param Options GAS 路由选项，例如是否忽略抑制、翻译或通知对象。
 */
void UCC_GameplayCueManager::RouteGameplayCue(AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
    const FGameplayCueParameters& Params, EGameplayCueExecutionOptions Options)
{
    if (Tag == CCDamageCueTags::DamageText)
    {
        // CueSet 使用 GetDefaultObject(false) 不主动创建对象，必须先构造原生 CDO 才能执行。
        GetDefault<UCC_GameplayCue_DamageText>();
        UGameplayCueSet* Set = GetRuntimeCueSet();
        const int32* Index = Set->GameplayCueDataMap.Find(Tag);
        if (!Index || *Index == INDEX_NONE)
        {
            Set->AddCues({FGameplayCueReferencePair(Tag, FSoftObjectPath(UCC_GameplayCue_DamageText::StaticClass()))});
            Index = Set->GameplayCueDataMap.Find(Tag);
        }
        if (Index && Set->GameplayCueData.IsValidIndex(*Index))
            Set->GameplayCueData[*Index].LoadedGameplayCueClass = UCC_GameplayCue_DamageText::StaticClass();
    }
    Super::RouteGameplayCue(Target, Tag, Event, Params, Options);
}
