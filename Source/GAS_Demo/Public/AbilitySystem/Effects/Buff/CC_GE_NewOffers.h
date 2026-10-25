#pragma once
#include "AbilitySystem/Effects/Buff/CC_GE_BuffDash.h"
#include "CC_GE_NewOffers.generated.h"

/**
 * 永久强攻（数值层）：每次获得增加 20 点攻击力，可叠加，倒地保留。
 * 对应 GE_Buff_AttackPermanent；通过 GAS Modifier 聚合数值，移除时由 GAS 撤回。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_AttackPermanent : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_AttackPermanent();
};

/**
 * 生命再生（数值层）：每秒恢复目标当前最大生命值的 1%，持续 5 秒，获得瞬间不回血。
 * 对应 GE_Buff_RegenPercent；GE 周期修改 Healing 属性，实际回血及上限限制由 AttributeSet 处理。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_RegenPercent : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_RegenPercent();
};

/**
 * 十秒霸体（状态层）：授予 CCTags.Status.SuperArmor，仍然承受伤害。
 * 对应 GE_Buff_SuperArmor；受击系统消费标签来跳过受击动作，GE 本身不处理受击逻辑。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_ArmorTen : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_ArmorTen();
};

/**
 * 目标身上的十秒魅惑（状态层）：授予 CCTags.Status.Charmed，阵营与索敌由现有系统处理。
 * 这是魅惑攻击命中后施加给敌人的子 GE，不是选项池中供玩家选择的 Buff。
 * 由 UCC_GE_CharmAttackTen 配置给命中规则，独立计时，父 Buff 结束不会提前解除它。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_CharmTen : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_CharmTen();
};

/**
 * 持有者身上的魅惑攻击（规则层）：持续 10 秒，有效伤害命中后给敌人施加十秒魅惑。
 * 对应 GE_Buff_CharmAttack；配置 ApplyEffectOnDamage Behavior，子效果为 UCC_GE_CharmTen。
 * 这里只声明规则模板，事件监听、命中过滤和子效果施加由独立 Behavior 负责。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_CharmAttackTen : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_CharmAttackTen();
};

/**
 * 永久分裂箭（规则层）：每轮 3 支箭，总散射角 30°，单箭伤害 60%，倒地保留。
 * 对应 GE_Buff_SplitArrow；继承已有分裂箭规则模板，只覆盖持续时间、层数上限和显示配置。
 * 与连续两箭共存时，每个连续轮次都使用分裂后的箭数；实际发射由弓箭系统执行。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_SplitPermanent : public UCC_GE_BuffSplitArrow
{
    GENERATED_BODY()
public:
    UCC_GE_SplitPermanent();
};

/**
 * 永久连续两箭（规则层）：每次攻击连续发射两轮，默认间隔 0.18 秒，倒地保留。
 * 对应 GE_Buff_DoubleArrow；配置 RepeatVolley Behavior，在通用放箭上下文中增加发射轮数。
 * GE 不创建箭或计时器，连续发射及取消清理由弓箭系统负责，可与分裂箭组合。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_DoubleArrowPermanent : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_DoubleArrowPermanent();
};

/**
 * 永久反弹箭（状态层）：授予 Ability.Deflect.Projectile，倒地保留，不叠加效果。
 * 对应 GE_Buff_DeflectArrow；挥砍有效帧检测并反弹敌方箭，由攻击与投射物系统执行。
 * 本类只配置资格标签，不监听伤害事件，不创建投射物或 Behavior。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_DeflectArrowPermanent : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCC_GE_DeflectArrowPermanent();
};
