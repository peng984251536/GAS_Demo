#pragma once
#include "GameplayCueNotify_Static.h"
#include "NativeGameplayTags.h"
#include "CC_GameplayCue_DamageText.generated.h"
/** 独立伤害/治疗数字标签，不再复用命中特效 Cue。 */
namespace CCDamageCueTags
{
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageText);
    /** 独立显示工具省略来源时的观看者标记，不修改正常伤害的筛选策略。 */
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ManualDisplay);
}
/** 客户端数字桥接：解析类型和数值，筛选本地观看者，再提交飘字子系统；不修改属性。 */
UCLASS()
class GAS_DEMO_API UCC_GameplayCue_DamageText : public UGameplayCueNotify_Static
{
    GENERATED_BODY()
public:
    /**
     * 设置独立伤害数字 Cue 标签，与声音和 Niagara 特效分离。
     */
    UCC_GameplayCue_DamageText();
    /**
     * 只响应单次 Executed 事件。
     * @param Type 本次 GameplayCue 事件类型。
     * @return 事件为 Executed 时返回 true，其他事件返回 false。
     */
    virtual bool HandlesEvent(EGameplayCueEvent::Type Type) const override;
    /**
     * 解析伤害/治疗样式，筛选本地观看者，并提交独立飘字条目。
     * @param Target 接收数字 Cue 的目标 Actor。
     * @param Params 数字 Cue 参数：位置、来源、数值以及扩展 Context。
     * @return 至少向一个本地玩家提交数字时返回 true。
     */
    virtual bool OnExecute_Implementation(AActor* Target,
        const FGameplayCueParameters& Params) const override;
};
