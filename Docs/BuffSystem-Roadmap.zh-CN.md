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

## 后续需求出现时再实现

- 通用输入配置如何绑定新能力：当前不修改 PlayerController 的输入策略。
- ApplyEffectOnDamage 规则（监听已实现的 DamageDealt / DamageTaken 事件）；ProcDepth 防循环字段尚未加入 Context。
- 近战等其他伤害来源的 Damage.Source.* 补齐；当前不扩大到全战斗流程。
- 按状态标签选择动作集、移动射击、冻结、魅惑等具体玩法。
- 爆炸箭、反弹、召唤规则及其专用基础设施。

实现代码已编译通过；已准备 GAS_Demo.Buff.FourExamples，按用户要求不再继续执行 UE 验证。后续实机测试由用户负责。
