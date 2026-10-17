#include "GameplayCues/CC_GameplayCueManager.h"
#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "Damage/CC_DamageTags.h"
#include "GameplayCueSet.h"

namespace
{
	/** 只对原生 Cue 的标签生效；其他 Cue 照常走 GAS 的资产查找。 */
	UClass* FindNativeCueClass(const FGameplayTag& Tag)
	{
		if (Tag == CCDamageTags::CueDamageText) return UCC_GameplayCue_DamageText::StaticClass();
		if (Tag == CCDamageTags::CueHealText) return UCC_GameplayCue_HealText::StaticClass();
		return nullptr;
	}
}

void UCC_GameplayCueManager::RouteGameplayCue(AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
	const FGameplayCueParameters& Params, EGameplayCueExecutionOptions Options)
{
	if (UClass* NativeCueClass = FindNativeCueClass(Tag))
	{
		// CueSet 用 GetDefaultObject(false) 取 Notify，不会主动创建 CDO，所以先确保它存在。
		NativeCueClass->GetDefaultObject();
		UGameplayCueSet* Set = GetRuntimeCueSet();
		const int32* Index = Set->GameplayCueDataMap.Find(Tag);
		if (!Index || *Index == INDEX_NONE)
		{
			Set->AddCues({FGameplayCueReferencePair(Tag, FSoftObjectPath(NativeCueClass))});
			Index = Set->GameplayCueDataMap.Find(Tag);
		}
		if (Index && Set->GameplayCueData.IsValidIndex(*Index)) Set->GameplayCueData[*Index].LoadedGameplayCueClass = NativeCueClass;
	}
	Super::RouteGameplayCue(Target, Tag, Event, Params, Options);
}
