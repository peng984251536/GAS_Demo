# NewBuff 选项池

目标资产：`/Game/GAS_Demo/GameplayEffects/NewBuff/DA_BuffOfferPool_New`。8 个选项，每轮抽 3 个，等级均为 1。

| 顺序 | 资产 | 分类与行为 |
|---|---|---|
| 1 | GE_Buff_AttackPermanent | 数值层：无限持续，攻击力 +20，重复获得累加，倒地保留 |
| 2 | GE_Buff_RegenPercent | 数值层：持续 5 秒，每秒治疗当前最大生命值的 1%，获得瞬间不治疗，满血截断 |
| 3 | GE_Buff_SuperArmor | 状态层：霸体标签 10 秒，仍承受伤害 |
| 4 | GE_Buff_CharmAttack | 规则层：10 秒内造成伤害后对存活敌人施加独立的 10 秒魅惑状态；排除 DoT；魅惑随施加者阵营，AI 复用现有清目标与重新索敌 |
| 5 | GE_Buff_SummonAlly | 能力层：复用已有资产，召唤弓箭手，保持现有 GA 与召唤配置 |
| 6 | GE_Buff_SplitArrow | 规则层：永久，每轮 3 支箭，总散射 30°，每支 60% 伤害；复用现有 SplitArrow Behavior |
| 7 | GE_Buff_DoubleArrow | 规则层：永久，每次攻击连续 2 轮，间隔 0.18 秒 |
| 8 | GE_Buff_DeflectArrow | 状态层：永久，挥砍有效帧将范围内敌方箭弹回，每支箭最多反弹一次，倒地保留 |

分裂箭与连续两箭都只有 1 层，可以共存，组合为连续两轮各 3 支箭。两种强化通过已有召唤 Buff 共享组件同步给弓箭手，尚未召唤时也可先获取。永久强化倒地保留，关卡结束按现有 BuffComponent 生命周期清理，不写入跨局存档。

规则层在服务器执行，标签由 GAS 复制，箭由服务器生成。分裂数量相加、散射取最大、单箭伤害倍率相乘；连续轮数相加（上限 8），间隔取最大。放箭时一次性解析本轮序列，之后移除强化不撤销已解析序列；死亡或打断射击会取消剩余轮次，动画正常完成则等待序列完成。已生成的箭保持原有伤害快照。

攻击魅惑结束、抑制、死亡时自动取消事件订阅。已经施加给目标的魅惑独立计时，不随攻击 Buff 消失；目标魅惑结束或被抑制时，现有阵营系统恢复其基础阵营。未新增技能、AI、伤害系统对具体 Buff 的判断。

原生 GE 类只提供可编辑默认配置，玩法由系统或独立 Behavior 实现。构建后执行 `Scripts/configure_new_buff_offers.py` 生成七个普通 GE 蓝图并配置池；脚本在 `Saved/BuffSystem/NewOffersBefore` 保留池的首次修改前备份，在 `Saved/BuffSystem/BeforeDeflectPool.uasset` 保留加入反弹前的池，不改已有召唤蓝图。

反弹归类为状态层开关，无额外 Behavior、事件订阅或轮询计时器。GE 只授予 `Ability.Deflect.Projectile`；近战判定在有效帧调用投射物交互系统，只处理攻击形状中的箭，空闲时无效果。检测统一使用原生 MeleeHitBox Notify 每招配置的 Shapes。敌我只通过 IsHostile 判定。资格由 GAS 复制，检测、归属切换及伤害结算仅由服务器执行，箭的位置继续复制给客户端。

反弹时保留原箭技能系数、固定伤害与倍率，按反弹者当时属性重新 PrepareDamage，随后同步替换伤害 Context、Owner、Instigator、碰撞忽略对象和飞行方向，添加 `Damage.Source.Deflected`。原射手重新成为可命中对象，伤害、击杀及伤害后 Buff 归反弹者。已反弹箭独立飞行，移除或抑制资格只阻止后续反弹；同一支箭不允许被再次反弹。分裂箭、两连射不自动改造已飞来的箭；它们仍只影响角色主动放箭，魅惑攻击等伤害后规则可以响应反弹箭的伤害。

自动化分组 `GAS_Demo.Buff.NewOffers`：`ConfigAndEffects` 检查八个选项有效、抽 3 项、攻击叠层及撤回、霸体抑制、分裂与连射组合及抑制恢复、真实伤害触发魅惑与订阅清理，以及 5 秒周期回血。`Deflect` 检查无资格、空闲、死亡、抑制时不反弹，恢复与 Notify 形状检测，友军过滤、单箭最多一次、转向与归属切换、移除 Buff 后保留飞行快照，以及真实碰撞伤害事件归属和魅惑组合。

2026-10-23 验证：GAS_DemoEditor Development 编译通过；资产生成命令成功；独立编辑器进程重新加载资产后，`GAS_Demo.Buff.NewOffers` 成功（1 通过、0 失败、用例内 0 警告/错误）；Buff Harness 检查 ERROR 0 / LEGACY 0。日志在 `Saved/BuffSystem/NewOffers*.log`，报告在 `Saved/Automation/NewBuffOffers/index.json`。未执行多人 PIE、动画中实际连射和随从共享的人工验收。

2026-10-23 反弹增量验证：最终编译通过；8 项资产池重新加载成功；`ConfigAndEffects` 与 `Deflect` 两项测试均通过（0 警告、0 错误）。报告在 `Saved/Automation/DeflectBuffFinal/index.json`，日志在 `Saved/BuffSystem/DeflectAutomationFinal.log`。测试世界用 FEditorScriptExecutionGuard 允许未初始化游戏世界的动态碰撞委托回调，测试调用实际 Notify 和箭的碰撞回调。尚未进行多人 PIE 与实机挥砍时序的人工验收。
