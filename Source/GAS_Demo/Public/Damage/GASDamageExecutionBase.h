#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "NativeGameplayTags.h"
#include "Damage/StandaloneDamageCalculator.h"
#include "GASDamageExecutionBase.generated.h"

/**
 * 一次伤害 Spec 的 SetByCaller 参数标识，定义和原生注册位于对应 .cpp。
 *
 * 技能通过 Spec.SetSetByCallerMagnitude(Tag, Value) 填写；不是用于给角色授予状态的 Tag。
 * 未填写时采用下面的默认值；填入的伤害值应为非负数，接入层最终统一取负数扣血。
 */
namespace StandaloneDamageTags
{
    /** 技能额外固定伤害，默认 0。 */
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(FlatDamage);
    /** 攻击力系数，默认 1；2 表示使用 200% 攻击力。 */
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillScale);
    /** 本次距离系数，默认 1；不会自动读取武器曲线。 */
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DistanceMultiplier);
    /** 本次部位/暴击等综合倍率，默认 1；命中判定由调用方完成。 */
    GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitMultiplier);
}

/**
 * GAS 桥接层：捕获已聚合的属性 -> 调用纯计算器 -> 输出 Health 扣减。
 *
 * 本类只认识 FGameplayAttribute，不引用具体角色、AttributeSet、队伍系统或项目单例。
 * 原生子类在构造时绑定属性，需要游戏规则时重写 PrepareDamageInput。
 * 抽象基类不能直接作为完整配置使用，GE 应选择已绑定属性的具体子类。
 *
 * 执行约定：
 * - 只在目标 ASC 的权威端输出伤害；不提供客户端预测扣血。
 * - 一次执行表示一次命中，不自动把伤害 GE 的 StackCount 当作命中次数。
 * - 属性 Buff 的叠加已经由 GAS 处理，这里不遍历活动 GE 再算一次。
 * - 输出的是 Health 的负向 Additive 修改；免伤回调、死亡及表现由项目其他部分负责。
 *
 * Execution 对象通常由多个 Spec 共用。只在构造时设置属性绑定，
 * 执行期间的目标、伤害、命中信息必须放在局部变量或 Spec/Context 中。
 */
UCLASS(Abstract)
class GAS_DEMO_API UGASDamageExecutionBase : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    /**
     * GAS 执行伤害 GE 时调用的入口，通常无需由业务代码手动调用。
     *
     * @param ExecutionParams 提供当前 Spec、攻防 ASC、属性捕获及作用域求值接口。
     * @param OutExecutionOutput 向 GAS 提交的属性修改；本类最多添加一条 Health 扣减。
     *
     * 配置错误、必需属性捕获失败、非法数值、规则拒绝或目标无血时不添加扣血输出。
     * 有效伤害先限制到目标当前生命值，再以负数输出；最终仍受 AttributeSet 回调约束。
     */
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
    /**
     * 绑定属性并登记 GAS 需要捕获的属性定义，仅在原生子类构造函数中调用。
     *
     * FGameplayAttribute 标识 AttributeSet 的某个字段，本身不是属性数值。
     * RelevantAttributesToCapture 登记完成后，GAS 才能为 Spec 准备相应捕获数据。
     *
     * @param SourceAttack 攻击方攻击力字段，必填；应表示数值攻击力，不是额外倍率。
     * @param TargetHealth 目标生命值字段，必填；直接读取当前值并作为扣血输出目标。
     * @param TargetDefense 目标固定防御字段，可省略。未绑定时防御为 0；已绑定但捕获失败则取消伤害。
     * @param bSnapshotAttack 是否快照攻击属性，默认 true。通常在 MakeOutgoingSpec 时捕获 Source；
     *        后续攻击 Buff 的变化不会更新已有快照。false 表示通过非快照捕获在执行时求值。
     *
     * 防御使用非快照捕获，按执行时的属性及求值条件计算。
     * Health 不注册为捕获属性：这里需要实际当前值作为扣血上限，避免 Execution 的
     * Scoped Modifier 改变用于上限判断的生命值。Health 输出仍会经过 GAS 的正常执行流程。
     */
    void ConfigureAttributes(
        const FGameplayAttribute& SourceAttack,
        const FGameplayAttribute& TargetHealth,
        const FGameplayAttribute& TargetDefense = FGameplayAttribute(),
        bool bSnapshotAttack = true);

    /**
     * 数值读取完成后、纯计算器执行前的项目规则扩展点。默认允许伤害且不修改输入。
     *
     * 典型用途：查询队伍/免伤、读取 EffectContext 的命中结果、通过武器接口求距离倍率。
     * 可以返回 false 取消本次伤害，也可以设置 InOutInput.bDamageAllowed=false 得到零伤害。
     * IncomingMultiplier 默认 1，需要承伤属性时，可在原生子类构造函数中额外登记捕获定义，
     * 再在此函数中调用 AttemptCalculateCapturedAttributeMagnitude 并填写输入。
     *
     * @param ExecutionParams 当前 Spec 和攻防 ASC 的访问入口；所需对象应检查有效性。
     * @param InOutInput 已填入攻击、防御和 SetByCaller 参数的当次局部输入，可按项目规则调整。
     * @return true 继续校验及计算，false 立即结束且不输出属性修改。
     *
     * 此钩子应只读取游戏状态并调整局部输入，不在这里扣血或修改共享 Execution 成员。
     * 通过此接口无需让通用桥接层包含具体角色、武器和团队系统的头文件。
     */
    virtual bool PrepareDamageInput(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        StandaloneDamage::FInput& InOutInput) const;

private:
    /** 攻击方属性的捕获定义，包含字段、Source 来源以及是否快照，不保存当次攻击数值。 */
    UPROPERTY()
    FGameplayEffectAttributeCaptureDefinition AttackCapture;

    /** 目标固定防御的捕获定义；只有 bHasDefense=true 时登记和求值。 */
    UPROPERTY()
    FGameplayEffectAttributeCaptureDefinition DefenseCapture;

    /** 用于读取目标实际生命值并提交扣血的字段标识；不是 Health 数值缓存。 */
    UPROPERTY()
    FGameplayAttribute HealthAttribute;

    /** 是否显式绑定了防御属性。false 是受支持的无防御配置，不是捕获失败。 */
    bool bHasDefense = false;
    /** 必需的攻击与生命值字段标识是否有效；不保证运行时 Actor 已注册对应 AttributeSet。 */
    bool bConfigured = false;
};
