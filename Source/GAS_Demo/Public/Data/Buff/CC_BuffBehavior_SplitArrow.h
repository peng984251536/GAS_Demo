#pragma once
#include "Data/Buff/CC_BuffBehavior.h"
#include "CC_BuffBehavior_SplitArrow.generated.h"

/**
 * 分裂箭只修改这次发射的上下文，不生成投射物、不保存射击状态。
 */
UCLASS()

class GAS_DEMO_API UCC_BuffBehavior_SplitArrow : public UCC_BuffBehavior
{
    GENERATED_BODY()

  public:
    UCC_BuffBehavior_SplitArrow();
    /**
	 * 每层额外箭数；合并：相加。
	 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|SplitArrow",
              meta = (ClampMin = "0", ClampMax = "31"))
    int32 ExtraArrows = 2;
    /**
	 * 水平散射总夹角；合并：取最大。
	 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|SplitArrow",
              meta = (ClampMin = "0", ClampMax = "180"))
    float SpreadAngle = 30.f;
    /**
	 * 每支箭的伤害比例；合并：相乘，同一个 Behavior 的层数只影响数量。
	 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff|SplitArrow",
              meta = (ClampMin = "0", ClampMax = "1"))
    float DamagePerArrowRatio = 0.6f;

  protected:
    /**
	 * 根据当前 GE 层数叠加额外箭数；抑制与结束由 BuffComponent 阻止钩子调用。
	 * @param Hook 当前决策点，必须是 Hook.Projectile.Fire。
	 * @param Context 本次射击的可修改参数；类型不符时跳过并记录警告。
	 */
    virtual void ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context) override;
};
