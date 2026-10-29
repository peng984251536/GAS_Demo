# 召唤 Buff（在玩家身边召唤随从）

获得召唤 Buff → 立即在玩家身边召唤一个与玩家**同阵营**的 `ACC_EnemyCharacter` → Buff 自动消失。
随从会复制玩家当前的 Buff；之后玩家每获得一次 Buff，随从也获得一次。
随从默认**跟随召唤它的玩家**：没有目标时回到玩家身边，只打玩家附近的敌人，追远了会放弃目标回来（见下文「跟随」）。

## 组成

| 类 | 职责 |
|---|---|
| `UCC_SummonAlly`（GA） | 带 `ActivateOnGiven`，授予即激活，只在服务器执行。从玩家身后开始按 45° 尝试 8 个方向（距离 `SpawnDistance`），生成 `SummonClass`；成功后下一帧移除授予它的 Buff。召唤物不能再召唤 |
| `UCC_GE_BuffSummon`（GE 原生父类，可选） | 无限持续、最多 1 层，授予召唤 GA，自带 `Buff.NotShared` 标签 |
| `ACC_BaseCharacter::SetTeamOverride` | 实例级阵营；召唤物继承玩家阵营，敌我判断、AI 选目标、击杀经验通过 `UCC_TeamStatics` 自动生效 |
| `UCC_SummonBuffShareComponent` | 召唤后加在随从身上（只在服务器）。只用 `UCC_BuffComponent` 的公开接口：`GetActiveBuffs` + `OnBuffsChanged` + `ApplyBuff` |
| `ACC_AIController::SetSquadOrder` | 召唤后给随从下达 `Follow`、锚点为玩家。栓绳与跟随的实现见 `AI-Combat.zh-CN.md` 第 6 节 |

## 跟随

| 情况 | 随从的行为 |
|---|---|
| 玩家附近（`EngageRadius` 800）没有敌人 | 黑板 `FollowAnchor` = 玩家，行为树跟随分支 MoveTo 玩家 |
| 敌人进入玩家 800 以内 | 选为目标去打，照常使用拥挤惩罚、回敬、被挡换目标 |
| 目标被引到离玩家超过 `LeashRadius`（1200） | 放弃目标，回到玩家身边 |
| 随从自己离玩家超过 `ReturnRadius`（1500） | 放弃任何目标，先回来 |
| 丢失目标 | 不去最后看到的位置搜索，直接回到玩家身边 |
| 玩家倒地等待复活 | 暂停跟随，按自由交战处理；复活后自动恢复 |

三个半径在 `FCC_SquadOrder` 里，召唤时用默认值。需要按随从类型区分时，改 `UCC_SummonAlly::SpawnNearCaster` 里下达的命令。

## Buff 同步规则

- **召唤时**：复制玩家当前全部 Buff（按层数）。
- **之后**：玩家每获得一次 Buff，随从也施加一次同样的 GE。新 Buff、叠层、刷新持续时间都算获得一次。
- **各自独立**：复制品按自己的持续时间结束；玩家的 Buff 到期、被驱散或玩家死亡，都不影响随从的。
- **不同步**：带 `Buff.NotShared` 资产标签的 Buff。召唤 Buff 必须带这个标签；漏标时召唤物也会拿到召唤 Buff，但不会召唤，那个 Buff 会一直留在召唤物身上。
- 随从死亡后不再接收新 Buff。
- 用 `ApplyBuffWithParameters` 施加的 SetByCaller 参数不会复制（展示快照里没有这些参数）。

## 编辑器接入

1. GA 蓝图（父类 `CC_SummonAlly`）：填 `Summon Class`；`Spawn Distance` 默认 150 cm。
2. GE 蓝图：授予上面的 GA，Removal Policy 建议 `Cancel Ability Immediately`，Stacking 最多 1 层，并在 **Asset Tags 里加 `Buff.NotShared`**。也可以直接把父类改成 `CC_GE_BuffSummon`，它已经带好这个标签。
3. 把 GE 加入 `DA_BuffOfferPool`。
4. 跟随：随从用的黑板（`AI_BB_Enemy`）加 Object 键 `FollowAnchor`（Base Class 选 Actor）；随从用的行为树（`AI_MeleeStance`，会被召唤的弓兵还有 `AI_BowStance`）在根 Selector **最后**加跟随分支，见 `AI-Combat.zh-CN.md` 7.2。

## 测试

- `GAS_Demo.Buff.Summon`：召唤 Buff 的授予、抑制、恢复、移除。
- `GAS_Demo.Buff.SummonShare`：复制现有 Buff、叠层同步、新 Buff 同步、独立存在、`Buff.NotShared` 不同步、随从死亡后不再接收。
- `GAS_Demo.Team.InstanceOverride`：阵营覆盖。
- `GAS_Demo.AI.SquadFollowLeash`：跟随栓绳的三个半径。
- 实机：生存模式里先拿几个 Buff 再拿召唤 Buff，看随从是否带上这些 Buff；之后再拿 Buff，看随从是否也获得。
