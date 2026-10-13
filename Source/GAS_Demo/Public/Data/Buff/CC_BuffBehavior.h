#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CC_BuffBehavior.generated.h"

class ACC_BaseCharacter;
class UCC_BuffDefinition;

/**
 * 项目特殊 buff 行为的生命周期扩展，用于玩法切换或额外资源管理。
 * 配置中的对象仅作为模板；角色组件为每个活动 GE 复制独立实例，在服务器执行回调。
 * 不替代 GAS 的原生触发组件，不在共享模板上保存角色状态。
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class GAS_DEMO_API UCC_BuffBehavior : public UObject
{
    GENERATED_BODY()
public:
    /**
     * 活动效果首次生效或从抑制状态恢复时执行，用于接入项目特殊玩法。
     * Character 与 InstanceId 标识本次角色实例，不能把它们保存到共享配置模板。
     */
    UFUNCTION(BlueprintNativeEvent, Category="Buff")
    void OnApplied(ACC_BaseCharacter* Character, const UCC_BuffDefinition* Definition, int32 InstanceId);
    virtual void OnApplied_Implementation(ACC_BaseCharacter*, const UCC_BuffDefinition*, int32) {}
    /**
     * 对应效果被抑制、到期或移除时执行，释放 OnApplied 创建的资源。
     * 此回调运行在服务器，客户端表现应通过复制对象或 GameplayCue 实现。
     */
    UFUNCTION(BlueprintNativeEvent, Category="Buff")
    void OnRemoved(ACC_BaseCharacter* Character, const UCC_BuffDefinition* Definition, int32 InstanceId);
    virtual void OnRemoved_Implementation(ACC_BaseCharacter*, const UCC_BuffDefinition*, int32) {}
    virtual UWorld* GetWorld() const override;
};
