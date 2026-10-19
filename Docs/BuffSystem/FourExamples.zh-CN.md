# 四种 Buff 的实际实现

| 层 | 资产（`/Game/GAS_Demo/Effects/Buff/`） | 行为 |
|---|---|---|
| 数值 | `GE_Buff_Attack` | 攻击力 +20，持续 10 秒；GAS Modifier 聚合与撤回 |
| 状态 | `GE_Buff_SuperArmor` | 授予 `CCTags.Status.SuperArmor`，持续 6 秒；现有 HitReact 读取标签，仍然承受伤害 |
| 能力 | `GE_Buff_Dash` | 原生 Grant Abilities 组件授予 `UCC_Dash`，持续 15 秒；冲刺速度 1800 cm/s，持续 0.2 秒 |
| 规则 | 原生 `CC_GE_BuffSplitArrow` | 持续 20 秒；每层增加 2 支箭，总散射角 30°，每支箭伤害 60%；同来源最多 3 层，重复施加刷新持续时间 |

当前收尾状态：C++ 实现已编译通过。用户负责后续 UE 实机测试，本次不再继续启动编辑器验证。强攻、霸体、冲刺蓝图已存在；本次生成的无效分裂箭蓝图已移至 `Saved/BuffSystem/InvalidAssets`，不参与游戏加载。可在 GE 类选择器直接选择原生 `CC_GE_BuffSplitArrow`，或在 UE 中以它为父类创建 `GE_Buff_SplitArrow` 蓝图。修正后蓝图的再次生成/加载尚未验证。

## 解耦边界

强攻和霸体复用已有 GE，不新建 Behavior。冲刺 GE 的原生父类 `UCC_GE_BuffDash` 只提供组件默认配置，不处理移动和输入；移动由 GA 的 GAS 根运动任务实现。分裂箭 GE 的配置父类 `UCC_GE_BuffSplitArrow` 为内嵌模板设置 Archetype 标志，确保蓝图编译和保存后仍保留模板；具体规则始终在 Behavior 中。

PlayerController 没有新增冲刺分支，也不自动替换闪避键。输入绑定由使用者决定，已获得能力的 ASC 可通过通用入口触发：

```cpp
ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(CCBuffTags::DashAbility));
```

蓝图可使用 **Try Activate Abilities by Tag**，标签为 `Ability.Movement.Dash`；也可向角色发送相同标签的 GameplayEvent。GE 授予能力不会自动执行冲刺。能力撤回后，相同入口无法再激活它。能力层使用本地预测；此次轻量验证不包含多人 PIE 实机检查。

分裂箭只在 `UCC_BuffBehavior_SplitArrow` 中修改通用 `FCC_ProjectileFireContext`。弓箭系统按 Build → Modify → Resolve → Execute 执行，只依赖 `Hook.Projectile.Fire` 与上下文，没有具体 Buff 判断。

BuffComponent 与 BuffBehavior 基类仅增加通用调试日志和注释，不保存分裂箭或冲刺的配置。没有加入召唤、爆炸箭或额外伤害事件系统。

## 共存与生命周期

- 强攻、霸体、冲刺、分裂箭可以共存，分别修改属性、状态、授予的能力和发射上下文。
- 分裂箭数量相加，散射角取最大，伤害倍率相乘。同一 GE 叠层只增加数量，不重复乘以伤害倍率；不同来源的规则实例分别参与合并。
- 1/2/3 层分别射出 3/5/7 支箭；数学展开对称散射，并限制最多 32 支。同一齐射的箭互相忽略碰撞。
- Buff 被抑制时，GAS 撤回冲刺 GA，BuffComponent 暂停分裂箭规则；恢复后重新授予 GA、启用原来的规则实例。
- 冲刺中移除 GE，立即取消 GA 并移除根运动源；正常冲刺结束也不修改永久移速。
- 移除分裂箭只影响后续发射。箭在发射时保存伤害参数和 GAS 攻击属性快照，命中通过 `UCC_DamageStatics::ApplyDamage` 统一结算；目标防御仍在命中时读取。
- 箭命中使用已有 `UCC_TeamStatics::IsHostile`，不新建阵营规则。每支箭最多结算一次命中，多支箭允许分别伤害同一目标，以单箭倍率平衡。

## 调试

Output Log 筛选 `LogCCBuff`。默认记录 `[Apply]`、`[Remove]`、`[Stack]`、`[Inhibition]`、`[Behavior]`、`[Dash]`、`[Fire]`、`[Arrow][Hit]`；日志含角色、实例号或句柄等定位信息。

需要钩子、参数合并和伤害快照细节时执行：

```text
log LogCCBuff Verbose
```

排查后恢复默认详细程度：`log LogCCBuff Log`。关键函数使用 `/** ... */` 注释，含参数说明及合并、快照、回收约定。

## 轻量验证

已准备 `GAS_Demo.Buff.FourExamples` 单个集成用例：验证数值/标签恢复、冲刺授予与激活及抑制撤回、原生分裂箭 GE 的层数与多来源合并、抑制恢复、移除恢复，以及发射后攻击 Buff 移除仍保持伤害快照。本次按用户要求未继续执行；最后调整的测试配置需要下一次项目编译。
