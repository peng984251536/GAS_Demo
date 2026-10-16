# 伤害与治疗系统

尽量交给 GAS 自带机制完成：Execution 计算、GE 的 Tag Requirements 拦截免疫、GE 上的 GameplayCue 负责飘字、GameplayEvent 通知 Buff。项目代码只有入口、计算、扣血三处。

## 链路

```
【服务器】
技能 / 投射物 ── FCC_DamageRequest
  ▼
① UCC_DamageStatics::ApplyDamage / PrepareDamage + ApplyPreparedDamage   唯一入口，校验一次
  │  DamageType、SourceTags 写进 Spec 动态标签；暴击策略、命中点写进 Context
  ▼
② CC_GE_Damage（Instant）
  │  ├ Tag Requirements：目标带 Status.DamageImmunity 时不生效
  │  ├ UCC_DamageExecution：读属性（按 Spec 标签求值）→ 暴击 → 公式 → 输出 Damage 中间属性
  │  └ GameplayCue.Combat.DamageText：数值取 Damage（理论伤害）
  ▼
③ UCC_AttributeSet：消费 Damage → 扣 Health
  │  → Event.Combat.DamageTaken 发给目标，Event.Combat.DamageDealt 发给来源
  ▼
【客户端】
④ UCC_GameplayCue_DamageText：命中点或目标头顶定位 → 筛选本地玩家 → 飘字子系统
```

治疗同理：`ApplyHealing` → `CC_GE_Healing`（Healing 中间属性 + `GameplayCue.Combat.HealText`）→ AttributeSet 加血并限制到最大生命。

| 文件 | 职责 |
| --- | --- |
| `Damage/CC_DamageStatics` | 唯一入口（CLAUDE.md 规则 3） |
| `Damage/CC_DamageTypes.h` | `FCC_DamageRequest`、暴击策略、应用结果 |
| `Damage/CC_DamageTags` | 全部伤害相关标签 |
| `Effects/Damage/CC_GE_Damage`、`CC_GE_Healing` | 伤害 / 治疗 GE |
| `Damage/CC_DamageExecution` | 读属性、暴击、调用公式 |
| `Damage/CC_DamageFormula.h` | 纯公式，可独立测试 |
| `Damage/CC_GameplayEffectContext` | Context 只多一个暴击标记 |
| `Damage/CC_DamageSettings` | 项目设置：使用哪个伤害 / 治疗 GE |
| `Attribute/CC_AttributeSet` | 扣血、加血、发伤害事件 |
| `GameplayCues/CC_GameplayCue_DamageText` | 伤害 / 治疗数字的客户端表现 |

## 调用

```cpp
#include "Damage/CC_DamageStatics.h"

FCC_DamageRequest Request;
Request.SkillScale = 2.f;                  // 200% 攻击力
Request.FlatDamage = 20.f;                 // 额外固定伤害
Request.DamageMultiplier = DistanceFalloff * WeakPoint; // 技能自己算好的额外倍率
Request.DamageType = TAG_Damage_Type_Fire; // 可选
Request.SourceTags.AddTag(TAG_Damage_Source_Melee);
Request.bHasHitResult = true;              // 有命中点时飘字从命中点出现
Request.HitResult = Hit;
UCC_DamageStatics::ApplyDamage(SourceASC, TargetASC, Request);
```

- 只在服务器调用；客户端调用返回 `NotAuthority`。
- 投射物：发射时 `PrepareDamage`（快照攻击方属性），命中时 `ApplyPreparedDamage(TargetASC, Snapshot, &Hit)`。
- 返回 `Applied` 只表示 GE 被接受；被免疫拦截返回 `Rejected`；参数非法返回 `InvalidInput`。
- 不要再手动飘字、手动发 Cue 或另外应用扣血 GE，GE 已经包含这些。

## 公式

```
(攻击 × SkillScale + FlatDamage) × (1 + OutgoingDamageBonus) × DamageMultiplier × 暴击倍率
  − Defense → 不低于 0 → × IncomingDamageMultiplier
```

- 攻击方属性（AttackPower、OutgoingDamageBonus、CriticalChance、CriticalMultiplier）在创建 Spec 时快照；目标属性（Defense、IncomingDamageMultiplier、Health）在应用时读取。
- 暴击：`CriticalPolicy` 为 Random 时按 CriticalChance 随机，Forced 必定暴击，Disabled 不暴击。结果写进 Context，客户端只显示。
- 目标已死亡（Health ≤ 0）时不输出伤害，因此也不会飘字、不会发事件。
- 输入非法或溢出时跳过并打 `LogCCDamage` 警告。

## 用 GE 扩展（不改代码）

| 想实现 | 怎么配 |
| --- | --- |
| 攻击 +10、增伤 50%、减伤 20%、防御 +30、暴击率 25% | GE 修改 AttackPower / OutgoingDamageBonus / IncomingDamageMultiplier / Defense / CriticalChance |
| 火焰抗性 30% | 受击方 GE：IncomingDamageMultiplier Additive −0.3，修正器 **Source Tags** 要求 `Damage.Type.Fire` |
| 投射物伤害 +20% | 攻击方 GE：OutgoingDamageBonus +0.2，修正器 Source Tags 要求 `Damage.Source.Projectile` |
| 对冻结目标增伤 | 攻击方 GE：OutgoingDamageBonus，修正器 **Target Tags** 要求冻结状态标签 |
| 无敌 / 免疫 | GE 授予 `Status.DamageImmunity` |
| 命中后触发（吸血、冻结、反伤） | 做一个 GA，Ability Trigger 设为 `Event.Combat.DamageDealt`（来源）或 `DamageTaken`（目标） |
| 周期伤害（DoT） | 派生 `CC_GE_Damage`，设置 Duration / Period，每跳独立结算和飘字 |
| 换飘字表现 | 改 Cue；或新建继承 `CC_GE_Damage` 的 GE 资产改 Cue 列表，在 项目设置 → Game → CC Damage 中指定 |

多个减伤 Buff 默认按 GAS 的 Additive 规则相加（两个 −0.2 得 0.6），需要独立相乘时要另外明确聚合方式。

## 伤害事件

`Event.Combat.DamageTaken`（发给目标）和 `Event.Combat.DamageDealt`（发给来源）在服务器扣血后发送，载荷是标准 `FGameplayEventData`：

| 字段 | 内容 |
| --- | --- |
| Instigator / Target | 来源 Avatar / 受击 Avatar |
| EventMagnitude | 实际扣血量（残血 5 挨 100 时为 5） |
| ContextHandle | 用 `FCC_GameplayEffectContext::Get` 读暴击；`GetHitResult` 读命中点 |
| InstigatorTags | Spec 标签，含伤害类型和来源标签 |

事件同步发出；在事件里再造成伤害会立即嵌套结算，做反伤一类效果时注意防循环（规划中的 ProcDepth）。

## 飘字

- 伤害数字显示**理论伤害**（Damage 中间属性），残血 5 挨 100 显示 100；治疗显示请求的治疗量。
- 有命中点时从命中点出现，否则从目标包围盒顶部上方 `HeadOffset`（默认 20cm）出现。
- 默认只显示给本地攻击者和本地受击者；项目设置 CC World Indicator 的 `Show All Damage Numbers` 显示全部。
- Cue 把数字交给每个本地玩家的 `UCC_DamageNumberSource`，显示、容量和自定义控件见 [WorldIndicator.zh-CN.md](WorldIndicator.zh-CN.md)。
- 伤害类型取 Spec 标签中第一个 `Damage.Type.*`，交给飘字控件选样式。
- 两个 Cue 是原生 C++ 类，由 `CC_GameplayCueManager` 在路由时注册，不需要蓝图资产。
- 独立显示工具 `UCC_DamageTextStatics::ShowDamageText` 只显示数字，不扣血；常规伤害不要再调用它。

## 配置

- `DefaultGame.ini`：`AbilitySystemGlobalsClassName=/Script/GAS_Demo.CC_AbilitySystemGlobals`、`GlobalGameplayCueManagerClass=/Script/GAS_Demo.CC_GameplayCueManager`。更换后需完整重启编辑器。
- 项目设置 → Game → CC Damage：`Damage Effect Class` / `Healing Effect Class`，默认原生类。

## 测试

- 纯公式：`Tests/Standalone/RunDamageCalculatorTests.cmd`（任意 C++17 编译器也可直接编译 `DamageCalculatorTests.cpp`）。
- UE 自动化：`GAS_Demo.Damage.Pipeline`（快照、公式、残血、死亡、免疫、按伤害类型的条件修正、事件、治疗）和 `GAS_Demo.Damage.CueAndContext`（Context 序列化、GE 自带 Cue 的数值、免疫不飘字、治疗 Cue、飘字分组）。

## 2026-10-16 重构说明

删除：`CC_CombatFeedbackComponent`、`FCC_DamageResolved` 与 AttributeSet / ASC 上的 `OnDamageResolved`、`StandaloneDamageCalculator.h`、`CC_GE_LyraStyleDamage`、旧的空 `CC_GE_Damage`、AttributeSet 中的免疫检查和飘字位置计算、`Data.StandaloneDamage.*` 标签。

改名：`IncomingDamage` / `IncomingHealing` → `Damage` / `Healing`；`ApplyDamage(Target, Snapshot)` → `ApplyPreparedDamage`；`DistanceMultiplier` + `WeakPointMultiplier` → `DamageMultiplier`；删除 `bUseHitLocation`（有命中点就用命中点）。SetByCaller 标签改为 `SetByCaller.Damage.*`。

蓝图里用到旧字段、旧属性名或旧节点的地方需要手动改；`BP_GA_Attack`、`GA_Attack` 请检查 Make CC Damage Request 的引脚。
