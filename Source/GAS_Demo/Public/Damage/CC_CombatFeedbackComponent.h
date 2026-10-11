#pragma once
#include "Components/ActorComponent.h"
#include "Damage/CC_DamageTypes.h"
#include "CC_CombatFeedbackComponent.generated.h"
class UCC_AbilitySystemComponent;
/** 服务器表现桥接：订阅结算通知，选择显示数值并发送独立伤害数字 Cue，不参与伤害计算。 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_CombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    /**
     * 配置事件驱动的反馈组件，不启用逐帧 Tick。
     */
    UCC_CombatFeedbackComponent();
    /**
     * 先撤销旧订阅，再绑定新的 ASC 结算通知；传 nullptr 解除绑定。
     * @param ASC 待绑定的项目 ASC；反馈组件传 nullptr 表示解绑。
     */
    void Initialize(UCC_AbilitySystemComponent* ASC);
    /** false 显示完整结算伤害；true 显示实际扣血。治疗总是显示实际恢复量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Feedback") bool bShowActualHealthDamage = false;
protected:
    /**
     * 离开世界时解除结算订阅，再交给父类清理。
     * @param Reason 引擎传入的结束原因。
     */
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    /**
     * 从结算结果选择显示值，构造独立数字 Cue；不再次计算或修改生命。
     * @param Result 本次伤害/治疗的不可变结果副本，包含完整伤害、实际生命变化与归属。
     */
    void HandleResult(const FCC_DamageResolved& Result);
    /** 当前订阅的 ASC，弱引用不延长其生命周期。 */
    TWeakObjectPtr<UCC_AbilitySystemComponent> BoundASC;
    /** 委托订阅句柄，初始化与结束时用于准确解绑。 */
    FDelegateHandle Handle;
};
