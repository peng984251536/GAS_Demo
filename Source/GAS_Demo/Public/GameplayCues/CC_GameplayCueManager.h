#pragma once
#include "GameplayCueManager.h"
#include "CC_GameplayCueManager.generated.h"
/** 项目 Cue 管理器：在真正路由时补注册原生数字 Cue，兼容编辑器/运行时资产库重新扫描。 */
UCLASS()
class GAS_DEMO_API UCC_GameplayCueManager : public UGameplayCueManager
{
    GENERATED_BODY()
public:
    /**
     * 分发前恢复原生数字 Cue 映射和默认对象，再交给 GAS 正常路由。
     * @param Target 接收当前 Cue 的目标 Actor。
     * @param Tag 当前要路由的 GameplayCue 标签。
     * @param Event 本次 GameplayCue 事件类型。
     * @param Params 需要分发的 GameplayCue 表现参数。
     * @param Options GAS 路由选项，例如是否忽略抑制、翻译或通知对象。
     */
    virtual void RouteGameplayCue(AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
        const FGameplayCueParameters& Params, EGameplayCueExecutionOptions Options = EGameplayCueExecutionOptions::Default) override;
};
