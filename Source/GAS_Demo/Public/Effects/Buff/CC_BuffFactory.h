#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CC_BuffFactory.generated.h"

class UAbilitySystemComponent;
class UCC_GE_Buff;

/**
 * 根据指定 buff 效果类创建应用实例的轻量工厂。
 * 配置初始化由效果加载流程完成；修饰器、触发条件和堆叠仍由 GAS 管理。
 */
UCLASS()
class GAS_DEMO_API UCC_BuffFactory : public UObject
{
    GENERATED_BODY()
public:
    /**
     * 根据来源 ASC、效果类与等级创建 Spec，保留 UE 自身的配置与应用流程。
     * @param Source 施加效果的来源，负责创建上下文和属性快照。
     * @param BuffEffect 场景道具或技能选定的独立 buff 效果类。
     * @param Level 效果等级，供原生可缩放数值和计算类使用。
     * @return 有效的效果 Spec；输入或配置无效时返回空句柄。
     */
    static FGameplayEffectSpecHandle MakeBuffSpec(UAbilitySystemComponent* Source,
        TSubclassOf<UCC_GE_Buff> BuffEffect, float Level = 1.f);
};
