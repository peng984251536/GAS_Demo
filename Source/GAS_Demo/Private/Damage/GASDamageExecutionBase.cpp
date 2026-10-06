#include "Damage/GASDamageExecutionBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

// 独立日志分类，便于从普通 GAS 日志中筛选配置、属性捕获或数值错误。
DEFINE_LOG_CATEGORY_STATIC(LogStandaloneDamage, Log, All);

// 原生注册后，C++ 和蓝图都可以使用这些 Tag，不需要修改项目的 GameplayTags.ini。
// 它们是伤害 Spec 的参数键；在这里注册不等于把 Tag 授予了任何角色。
namespace StandaloneDamageTags
{
    UE_DEFINE_GAMEPLAY_TAG(FlatDamage, "Data.StandaloneDamage.FlatDamage");
    UE_DEFINE_GAMEPLAY_TAG(SkillScale, "Data.StandaloneDamage.SkillScale");
    UE_DEFINE_GAMEPLAY_TAG(DistanceMultiplier, "Data.StandaloneDamage.DistanceMultiplier");
    UE_DEFINE_GAMEPLAY_TAG(HitMultiplier, "Data.StandaloneDamage.HitMultiplier");
}

void UGASDamageExecutionBase::ConfigureAttributes(
    const FGameplayAttribute& SourceAttack,
    const FGameplayAttribute& TargetHealth,
    const FGameplayAttribute& TargetDefense,
    bool bSnapshotAttack)
{
    // 清理本基类负责的捕获列表，再按当前绑定重建。
    // 子类如果还要捕获暴击率、承伤倍率等属性，应先调用本函数，再添加额外定义。
    RelevantAttributesToCapture.Reset();
    HealthAttribute = TargetHealth;
    // 这里只检查字段标识。攻击方/目标是否真的注册了对应属性集，要在执行时检查。
    bConfigured = SourceAttack.IsValid() && TargetHealth.IsValid();
    bHasDefense = TargetDefense.IsValid();
    if (!bConfigured)
    {
        return;
    }

    // Source 表示从攻击方读取；快照选项决定创建 Spec 后属性变化是否影响本次伤害。
    // 必须加入 RelevantAttributesToCapture，否则 Spec 不会按本定义准备捕获数据。
    AttackCapture = FGameplayEffectAttributeCaptureDefinition(
        SourceAttack, EGameplayEffectAttributeCaptureSource::Source, bSnapshotAttack);
    RelevantAttributesToCapture.Add(AttackCapture);
    if (bHasDefense)
    {
        // 防御从受击者读取，不快照，使执行时求值能够考虑目标当前的防御状态。
        DefenseCapture = FGameplayEffectAttributeCaptureDefinition(
            TargetDefense, EGameplayEffectAttributeCaptureSource::Target, false);
        RelevantAttributesToCapture.Add(DefenseCapture);
    }
}

bool UGASDamageExecutionBase::PrepareDamageInput(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    StandaloneDamage::FInput& InOutInput) const
{
    // 通用层不知道项目中的队伍、无敌 Tag 或武器类型，所以默认不附加规则。
    // 项目子类可以重写此函数，拒绝伤害或调整当次输入，而不改变通用执行流程。
    return true;
}

void UGASDamageExecutionBase::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
    // 1. 限制实际结算位置。
    // WITH_SERVER_CODE 排除不包含服务器代码的构建；运行时还需检查目标 ASC 是否有权威。
    // 客户端仍可播放预测表现，但本实现不会在客户端预测修改 Health。
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
    if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative())
    {
        return;
    }
    // 2. 必需绑定必须有效，受击者必须具有 Health 所属的属性集。
    // 配置错误时放弃本次伤害，不自行创建属性集，也不假定目标一定是某种角色。
    if (!bConfigured || !TargetASC->HasAttributeSetForAttribute(HealthAttribute))
    {
        UE_LOG(LogStandaloneDamage, Warning, TEXT("%s: Missing attribute binding or target Health."), *GetName());
        return;
    }

    // 3. 获取本次应用的运行时 Spec。GE 类/资产保存定义，Spec 保存本次参数和捕获数据。
    // 传入 Spec 中的 Source/Target Tag，供 GAS 判断哪些带条件的属性 Modifier 参与求值。
    // 这里不重新扫描角色全部活动 GE，也不手动重做 Modifier 的叠层或乘法聚合。
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    FAggregatorEvaluateParameters Evaluation;
    Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    float Attack = 0.0f;
    float Defense = 0.0f;
    // 4. 读取 GAS 捕获的属性求值结果，包含符合当前求值条件的加成，
    // 也允许 GE 为本次 Execution 配置作用域修正。不能再把相同 Buff 加一次。
    // 未绑定防御时保留默认 0；绑定后却读取失败是错误，不能悄悄按无防御计算。
    if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AttackCapture, Evaluation, Attack)
        || (bHasDefense && !ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DefenseCapture, Evaluation, Defense)))
    {
        UE_LOG(LogStandaloneDamage, Warning, TEXT("%s: Failed to capture Attack or Defense; damage skipped."), *GetName());
        return;
    }

    // 5. Health 在输出前从 ASC 直接读取，用实际当前值限制本次最多扣多少血。
    // 它不经过本次 Execution 的捕获作用域修正；已死亡或生命值非法时跳过。
    // 本类不会负责复活、死亡事件或血条刷新，这些由项目的属性回调/组件处理。
    const float Health = TargetASC->GetNumericAttribute(HealthAttribute);
    if (!FMath::IsFinite(Health) || Health <= 0.0f)
    {
        return;
    }

    // 6. 把 GAS 数据转换成纯计算器的输入，纯计算器不需要知道 Spec/ASC 的存在。
    // SetByCaller 的 false 表示参数缺失时不警告，因为下面为每项提供了合法默认值。
    // 默认就是“攻击力本身作为伤害”；FlatDamage/SkillScale 可以组成技能自己的公式。
    // 参数必须由服务器认可的技能/命中结果产生；这里只做数值校验，不验证命中真实性。
    StandaloneDamage::FInput Input;
    Input.AttackPower = Attack;
    Input.Defense = Defense;
    Input.FlatDamage = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, false, 0.0f);
    Input.SkillScale = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, false, 1.0f);
    Input.DistanceMultiplier = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::DistanceMultiplier, false, 1.0f);
    Input.HitMultiplier = Spec.GetSetByCallerMagnitude(StandaloneDamageTags::HitMultiplier, false, 1.0f);
    // 7. 给项目规则最后一次调整局部输入的机会，例如禁用队友伤害、补充承伤倍率。
    // 返回 false 即取消本次执行；输入的最终合法性仍由随后调用的纯计算器检查。
    if (!PrepareDamageInput(ExecutionParams, Input))
    {
        return;
    }

    // 8. 执行纯数值计算。读取失败、非法参数和算术溢出都不能变成一笔有效扣血。
    const StandaloneDamage::FResult Result = StandaloneDamage::Calculate(Input);
    if (!Result.IsValid())
    {
        UE_LOG(LogStandaloneDamage, Warning, TEXT("%s: Invalid damage input or arithmetic overflow; damage skipped."), *GetName());
        return;
    }

    // 9. 将理论伤害转换成拟提交的扣血量。例如理论伤害 100、目标剩 5 血，只提交 -5。
    // 在 double 中裁剪后再转 float，避免超大理论伤害转换时变为 Inf。
    // AttributeSet 后续仍可能拒绝/调整此次修改，因此 DamageToApply 不是最终掉血事件。
    const float DamageToApply = static_cast<float>(FMath::Min(Result.FinalDamage, static_cast<double>(Health)));
    if (DamageToApply > 0.0f)
    {
        // 本 GE 每次执行只表示一次命中。告诉 GAS 不要再把输出数值乘以 GE StackCount，
        // 避免已经裁剪到生命值的伤害被二次放大。攻击 Buff 自身的层数已在属性聚合中处理。
        OutExecutionOutput.MarkStackCountHandledManually();
        // 10. 声明“对目标 Health 加一个负数”，由 GAS 执行属性修改和相应 AttributeSet 回调。
        // 不在此调用 SetHealth，不接管死亡、受击动画、飘字，也不操作具体角色类型。
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            HealthAttribute, EGameplayModOp::Additive, -DamageToApply));
    }
#endif
}
