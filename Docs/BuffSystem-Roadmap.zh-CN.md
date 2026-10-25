# Buff 系统基础设施现状与待办

此表以当前代码为准。实际四种 Buff 见 [FourExamples.zh-CN.md](BuffSystem/FourExamples.zh-CN.md)。

## 已具备

| 能力 | 实现 |
|---|---|
| GE 原生持续时间、堆叠、修饰器和标签 | 普通 GE 蓝图 + 原生 GE 组件 |
| 显示和规则配置分离 | UCC_BuffUIData、UCC_BuffRulesComponent；旧 Definition / Factory 已删除 |
| 每个 ActiveGE 一份服务器 Behavior | Initialize / Enable / Disable / Shutdown，支持抑制与恢复 |
| 规则资源对称回收 | Behavior 基类管理事件订阅、计时器、Actor 与子 GE |
| Buff 列表复制 | FCC_ReplicatedBuffArray FastArray |
| 四种示例 | 强攻、霸体、临时冲刺蓝图；分裂箭原生 GE 配置（蓝图生成待 UE 内确认） |
| 原生临时能力授予 | Grant Abilities 组件授予 UCC_Dash，撤回时立即取消；输入由通用 GAS 入口触发 |
| 放箭决策点 | Hook.Projectile.Fire，FCC_ProjectileFireContext，Build → Modify → Resolve → Execute |
| 分裂箭规则 | 数量相加、角度取最大、倍率相乘，支持叠层和不同来源共存 |
| 箭的发射快照 | FCC_ArrowPayload + GAS 攻击属性快照；命中通过 DamageStatics::ApplyPreparedDamage |
| 投射物敌我查询 | 复用 UCC_TeamStatics::IsHostile |
| 伤害来源元数据 | FCC_DamageRequest.SourceTags / DamageType 写入 Spec 动态标签，投射物填写 Damage.Source.Projectile；GE 修正器可按这些标签做条件增减伤 |
| 伤害后事件 | AttributeSet 扣血后发送 Event.Combat.DamageDealt（来源）/ DamageTaken（目标），EventMagnitude 为实际扣血，Context 带暴击与命中点 |
| 调试 | LogCCBuff；生命周期 Log，钩子及快照 Verbose |
| 获取：三选一 | UCC_BuffOfferPool + UCC_BuffSelectionComponent（PlayerController 上，服务器抽取与校验）+ UCC_BuffSelectionScreen；见 [BuffSelection.zh-CN.md](BuffSelection.zh-CN.md) |
| 复活恢复 | `UCC_BuffComponent::ResumeAfterRevive`：撤销 `ClearBuffs(OwnerDeath)` 对保留实例（`bRemoveOnDeath=false`）的挂起，按 GAS 当前状态重新启用钩子与 Behavior；测试 `GAS_Demo.Buff.ResumeAfterRevive` |
| 死亡与击杀者 | `UCC_AttributeSet::OnOutOfHealth`（带 Instigator / Causer）→ `ACC_BaseCharacter::HandleOutOfHealth` → `UCC_GameEventSubsystem::OnCharacterDied`；"击杀触发"类规则以后从这里接入 |
| 实例级阵营 | `ACC_BaseCharacter::SetTeamOverride`（复制），`UCC_TeamStatics` 优先读取；召唤物继承召唤者阵营；测试 `GAS_Demo.Team.InstanceOverride` |
| 召唤 Buff | `UCC_SummonAlly`（授予即激活）在玩家身边召唤，成功后移除 Buff；`UCC_SummonBuffShareComponent` 把玩家的 Buff 同步给随从（各自独立计时，`Buff.NotShared` 不同步）；测试 `GAS_Demo.Buff.Summon` / `SummonShare`，见 [SummonAlly.zh-CN.md](SummonAlly.zh-CN.md) |
| 三选一服务器事件 | `UCC_BuffSelectionComponent::OnServerPendingChanged / OnServerChoiceAccepted`，见 [BuffSelection.zh-CN.md](BuffSelection.zh-CN.md) |

## NewBuff 选项池补充

- `UCC_BuffBehavior_ApplyEffectOnDamage` 监听 DamageDealt，排除 DoT、友军和已死亡目标，施加独立子 GE；目前配置为不造成伤害的魅惑状态，不需要伤害递归链。
- `UCC_BuffBehavior_RepeatVolley` 在现有 Projectile.Fire 上相加 VolleyCount、取最大 VolleyInterval；BowShoot 顺序执行，取消能力时清理未发射轮次。
- 反弹资格 `Ability.Deflect.Projectile`：状态 GE 授予标签；`CCProjectileInteractions::DeflectInShape` 在近战有效帧查询箭，`ACC_ArrowProjectile::Deflect` 完成交接、转向和来源快照重建。检测统一由原生 MeleeHitBox Notify 的 Shapes 驱动；每支箭最多反弹一次。
- 新资产配置与验收说明见 [NewBuffOffers.zh-CN.md](BuffSystem/NewBuffOffers.zh-CN.md)。

## 后续需求出现时再实现

- 能造成伤害的事件反应规则需要先补 ProcDepth 防循环字段；当前 ApplyEffectOnDamage 只用于不造成伤害的状态效果。
- 近战等其他伤害来源的 Damage.Source.* 补齐；当前不扩大到全战斗流程。
- 按状态标签选择动作集、移动射击、冻结等具体玩法；魅惑复用已有阵营查询与 AI 状态订阅。
- 爆炸箭、反弹分裂等额外组合规则；基础攻击反弹已具备。持续型召唤（Buff 结束时召唤物消失）如有需要再做 `UCC_BuffBehavior_Summon`；瞬时召唤已由 `UCC_GE_BuffSummon` 实现。

实现代码已编译通过；已准备 GAS_Demo.Buff.FourExamples，按用户要求不再继续执行 UE 验证。后续实机测试由用户负责。
