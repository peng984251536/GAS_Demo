#pragma once

#include "Effects/CC_GameplayEffectBase.h"
#include "CC_GE_Buff.generated.h"

class UCC_BuffDefinition;

/**
 * 场景道具实际应用的 buff 效果，继承项目现有 GameplayEffect 基类。
 * 加载或编辑器编译时，从指定配置初始化 UE 原生效果字段；拾取时仅创建 Spec。
 * 每种 buff 使用独立效果蓝图类，GAS 按效果类处理堆叠，不在角色组件中重复实现规则。
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class GAS_DEMO_API UCC_GE_Buff : public UCC_GameplayEffectBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff")
    TSubclassOf<UCC_BuffDefinition> BuffConfiguration;
    /**
     * 获取本效果引用的共享配置默认对象，只供读取显示信息和特殊玩法设置。
     * 不返回某个角色的运行时 buff 实例。
     */
    UFUNCTION(BlueprintPure, Category="Buff")
    const UCC_BuffDefinition* GetBuffConfiguration() const;
    virtual void PostInitProperties() override;
    virtual void OnGameplayEffectChanged() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
private:
    bool bInitializingFromConfiguration = false;
    /**
     * 在效果加载或编辑器更新阶段，将配置中的 UE 原生字段初始化到本效果。
     * GE 组件复制到本效果名下，保证组件通过 Outer 获取正确的所有者。
     * 拾取与技能施加过程中不调用此函数，避免修改正在使用的共享效果模板。
     */
    void InitializeFromConfiguration();
};
