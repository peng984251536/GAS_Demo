# 生存模式使用说明（波次刷怪 + 共享经验 + 倒计时复活）

设计原理、类图和时序图见 `Docs/Gameplay/SurvivalMode.zh-CN.md`（第 0 节是实现与设计的差异）。本文只讲怎么接入、怎么调、出问题怎么查。

> 状态：C++ 已写完，**尚未在 UE 中编译与实机验证**。第一次编译若报错，按报错修正后再按本文接线。

## 1. 它做了什么

| 部分 | 效果 | 由谁负责 |
|---|---|---|
| 波次 | 定时推进波次；按档位和存活人数算数量；选离玩家足够远的刷怪器；出生前播预警，分帧生成；回收长时间远离玩家的怪 | `UCC_WaveDirectorComponent`（挂在 GameState 上，自动） |
| 怪物成长 | 档位换初始化 GE（质变），GE 等级 = 波次（档内平滑成长），血量按人数缩放 | 配置 + `ACC_EnemyCharacter::InitializeAttributes` |
| 共享经验 | 玩家方击杀给全队加经验；每升一级，每个玩家各弹一轮三选一（排队） | `UCC_TeamProgressionComponent` + 已有的三选一 |
| 复活 | 阵亡后倒计时，在随机存活队友身边复活；复用原 Pawn，升级 Buff 保留 | `UCC_RespawnComponent`（挂在 GameMode 上） |
| 结算 | 全员阵亡即结束：停刷怪、清三选一、冻结怪物，客户端打开结算页，本地保存最佳纪录 | `ACC_SurvivalGameMode` + `ACC_SurvivalGameState` + 结算页控制器 |
| 统计 | 个人 / 全队：击杀、阵亡、复活、造成 / 承受伤害、已选 Buff、刷出怪数 | PlayerState / GameState 的 TagStack |

蓝图只需要**配置资产、摆刷怪器、做两个界面**。刷怪、经验、复活、结算都不需要蓝图逻辑。

## 2. 接入清单

按顺序完成。

### 2.1 编译

`Build.cs` 已新增 `ModularGameplay`（插件已在 .uproject 启用）。重新生成项目文件后编译。新增源码在 `Source/GAS_Demo/{Public,Private}/Game/Survival/`、`UI/Survival/`、`Subsystems/CC_GameEventSubsystem.*`、`Data/CC_EnemySpawnTypes.h`。

### 2.2 怪物：初始化 GE 与经验

每档怪物的属性来自一个**初始化 GE**（瞬时，Override 各项属性）。要让同一档内每波变强：

1. 新建 CurveTable（建议从 CSV 导入，例如 `CT_EnemyStats`），每行一条曲线，横轴是**波次**：

   ```csv
   Name,1,5,10,20
   EnemyHealth_T1,100,160,240,400
   EnemyAttack_T1,10,14,20,32
   EnemyHealth_T2,300,400,560,900
   ```

2. 初始化 GE 的修正器：Magnitude 选 **Scalable Float**，Value 填 1，Curve 选上表的行。施加时等级 = 波次，所以第 7 波读曲线在 X=7 处的值。
3. 已有的常数初始化 GE 也能直接用，只是档内不成长。
4. 每个怪物的 `CharacterConfig` → **Experience Reward**（默认 10）：被玩家方击杀时的基础经验，实际值再乘档位的经验倍率。玩家角色的配置可以填 0。

### 2.3 Buff 池：升级用的 GE 必须保留到死亡后

三选一池（`CC_BuffOfferPool`）里的每个 GE，在 **CC Buff Rules** 组件中**关闭 `bRemoveOnDeath`**。否则玩家阵亡时这些 Buff 会被移除，复活后不会恢复。配置资产的数据校验会对不符合的条目给出警告。

### 2.4 配置资产 `DA_SurvivalMode`

内容浏览器 → 右键 → 杂项 → 数据资产 → `CC_SurvivalModeConfig`。

**Waves（波次节奏）**

| 字段 | 默认 | 说明 |
|---|---|---|
| First Wave Delay | 5 秒 | 开局到第一波 |
| Wave Interval | 20 秒 | 两波之间，定时推进，不等清场 |
| Max Alive Enemies | 60 | 场上 + 待生成队列的上限，超出时本波少刷 |
| Advance When Cleared | 关 | 清场后立即下一波 |
| Tiers | — | 档位列表，**第一档 Start Wave 必须为 1，后续严格递增** |

**每个档位（Tier）**

| 字段 | 说明 |
|---|---|
| Start Wave | 从第几波开始用本档 |
| Init Effect | 本档初始化 GE（2.2）；为空时用怪物自己的 CharacterConfig |
| Enemies | 怪物蓝图 + 权重，按权重随机 |
| Base Count / Count Per Wave | 本档第一波的基础数量（整数，尚未乘人数倍率）/ 档内每波增加量（可小数） |
| Experience Multiplier | 本档经验倍率 |

**Spawning（刷怪执行）**

| 字段 | 默认 | 说明 |
|---|---|---|
| Spawns Per Tick / Tick Interval | 2 / 0.1 秒 | 分帧生成速度：每 0.1 秒最多 2 只 |
| Telegraph Duration | 0.8 秒 | 落点确定到真正生成的间隔；0 表示不预警 |
| Telegraph Cue | 空 | 预警特效的 GameplayCue（客户端本地执行，Location = 落点，Raw Magnitude = 时长） |
| Min Distance To Players | 800 | 刷怪器离最近存活玩家至少多远才可用 |
| Preferred Distance To Players | 1600 | 优先选离所有玩家都超过此距离的刷怪器（俯视角近似"屏幕外"）；0 表示不区分 |
| Project To Navigation | 开 | 落点必须能投影到 NavMesh |
| Leash Distance / Leash Time | 4000 / 8 秒 | 离所有玩家都超过此距离、持续此时长的怪会被回收（不给经验）；距离 0 表示不回收 |

**Player Scaling（人数缩放）**：`Count Multiplier`、`Health Multiplier` 两条曲线，X = 存活玩家数，Y = 倍率。**曲线没有关键帧时视为 1**。建议：数量 1→1.0、2→1.6、4→2.6；血量 1→1.0、2→1.3、4→1.8。人数在每波开始时取样。

**每波数量是所有受控刷怪器共享的总量**，不是每个刷怪器各生成一次。计算公式：

```text
计划数量 = max(0, 四舍五入(
    (Base Count + Count Per Wave × max(0, 当前波次 - Start Wave))
    × Count Multiplier(存活玩家数)
    × cc.Survival.SpawnCountScale))
可生成数量 = min(计划数量, max(0, Max Alive Enemies - 当前存活数量 - 待生成数量))
```

例如第一档 Start Wave=1、Base Count=5、Count Per Wave=1，单人 Count Multiplier 在 X=1 时为 1，且控制台倍率为 1，则前 3 波分别计划生成 5、6、7 只。摆两个刷怪器不会变成 10、12、14 只。若数量倍率为 0.2，前两波都会四舍五入为 1 只；单人也会应用该倍率，必须检查 X=1 处的实际值。

`Spawns Per Tick` 只控制队列每次处理的生成上限，不控制每波总数。每只怪按距离规则选择一个刷怪器，不保证均分，也不保证每个刷怪器每波都被使用。

**Progression（经验与升级）**

| 字段 | 默认 | 说明 |
|---|---|---|
| Experience To Next | 空曲线 | X = 当前等级，Y = 升下一级所需经验；有关键帧时优先使用 |
| Base Experience To Next / Per Level | 20 / 10 | 曲线为空时的线性公式：1→2 需 20，2→3 需 30… |
| Buff Pool | 空 | 三选一池（2.3）；为空时升级不弹三选一 |
| Max Level | 0 | 等级上限，0 不限 |
| Choosing Protection Effect | 空 | 联机时选卡期间施加的无限时长 GE，例如 `IncomingDamageMultiplier × 0.5`。单机选卡会暂停世界，不施加 |

**Respawn（复活）**

| 字段 | 默认 | 说明 |
|---|---|---|
| Base Delay / Delay Per Death / Max Delay | 5 / 2 / 20 秒 | 第 N 次阵亡等待 = Base + PerDeath × (N-1)，封顶 Max |
| Min Radius / Max Radius | 150 / 400 | 复活点离队友的距离范围 |
| Death Finished Timeout | 5 秒 | 倒计时结束后死亡动作仍未播完时最多再等多久（玩家没有死亡能力时的兜底） |
| Revive Effect | 空 | 复活时施加，建议：Health 回满 + 授予 `Status.DamageImmunity` 2 秒 + 复活 Cue。为空时代码兜底回满血 |

> 复活无敌请用 `Status.DamageImmunity`（伤害 GE 已按它拒绝施加），不要用 `Status.Invincible`（本项目里它用于抑制 Buff）。

### 2.5 场景：刷怪器

把场景里的 `CC_EnemySpawner` 勾选 **Managed By Wave Director**。勾选后它不再自动生成，`Enemy Class`、`Spawn Count` 也不再使用，只提供位置和半径。

- 至少摆 4～6 个，分散在地图四周，保证无论玩家在哪都有离得够远的刷怪器
- 半径内要有静态地面和 NavMesh（Project To Navigation 开启时）
- 没有任何勾选的刷怪器时，日志会警告且不刷怪

### 2.6 GameState 蓝图与结算页

1. 新建控件蓝图 `WBP_SurvivalResult`，父类 **CC_ActivatableWidget**，Class Defaults → **Controller Class** 选 `CC_SurvivalResultController`，Input Config 选 Menu，**取消 Allow Back**（必须用按钮离开）。
2. 实现 **On UI Model Changed**：把 Model 转成 `CC_SurvivalResultModel`，调用 **Get Result**，显示：

   | 字段 | 内容 |
   |---|---|
   | Wave / Team Level / Team Kills / Survival Time | 全队结果 |
   | Kills / Deaths / Revives / Damage Dealt / Buffs Chosen | 本地玩家 |
   | Best Wave / Best Kills / Best Survival Time，bNewBest* | 本地最佳纪录（`bRecordLoaded` 为 true 后有效） |
   | bCanControlMatch | 是否显示"再来一局 / 返回大厅"（房主或单机） |

3. 按钮：通过 **Get Screen Controller** 转成 `CC_SurvivalResultController`，调用 **Request Play Again** / **Request Return To Lobby**。非房主显示"等待房主"。
4. 新建蓝图 `BP_SurvivalGameState`，父类 **CC_SurvivalGameState**，设置 **Result Screen Class** = `WBP_SurvivalResult`，Result Screen Layer 默认 `UI.Layer.Menu`。

### 2.7 HUD：读取生存数据

现有战斗主界面（父类 `CC_CombatMainWidget`）的 **On UI Model Changed** 里，Model 转成 `CC_CombatMainModel` 后多读一项 **Get Survival**：

| 字段 | 用法 |
|---|---|
| bActive | false 表示不是生存模式地图，隐藏整个生存面板 |
| Current Wave / Alive Enemies / Team Kills / My Kills | 直接显示 |
| Team Level / Team Experience / Experience To Next Level | 等级与经验条 |
| Next Wave Server Time | 下一波倒计时：每帧用 GameState 的 **Get Seconds Until Server Time** 计算 |
| Life State / Respawn Server Time | `Dead` / `Dying` 时显示复活倒计时（同上计算） |

横幅类提示（"第 5 波""全队升级""某某阵亡 / 复活"）绑定 GameState 的 **On Match Message**，按 `Verb` 区分：`Survival.Msg.WaveStarted`（Magnitude = 波次）、`LevelUp`（Magnitude = 等级）、`PlayerDown` / `PlayerRevived`（Target = 玩家）、`MatchStarted`、`MatchEnded`。这些消息不可靠，只用于提示。 默认已由 `UCC_SurvivalToastRelay` 转成提示弹窗，文案与开关在项目设置 → CC Toast，见 [ToastNotification.zh-CN.md](../UI/ToastNotification.zh-CN.md)。

出生预警也可以在蓝图里画：波次组件的 **On Spawn Telegraph**（Location、Duration），与 Telegraph Cue 二选一或同时用。

### 2.8 GameMode 蓝图

新建 `BP_SurvivalGameMode`，父类 **CC_SurvivalGameMode**：

| 设置 | 值 |
|---|---|
| Config | `DA_SurvivalMode` |
| Default Pawn Class / Player Controller Class / HUD 等 | 照抄原战斗 GameMode 蓝图 |
| Game State Class | `BP_SurvivalGameState` |
| Player State Class | `CC_SurvivalPlayerState`（或其蓝图子类，**不要**换回原来的 PlayerState） |
| Start Timeout | 10 秒：等所有玩家 Pawn 就绪的最长时间 |

然后：

- **联机**：房间设置里的战斗地图使用这个 GameMode（World Settings → GameMode Override，或地图 URL 的 `?game=`）。
- **单机**：单机入口打开的地图同样改用它。
- 中途加入遵循房主的"允许中途加入"选项；允许时新玩家在队友身边出生，并补发 `等级 - 1` 轮三选一。

### 2.9 玩家死亡能力（建议）

玩家的 `CharacterConfig` 授予 `CC_ListenToHealth` + `CC_Death`（与敌人相同）并配置 `DeathActionData`：阵亡时播放死亡动作，播完后视角切到存活队友。`CC_Death` 只销毁敌人，玩家 Pawn 会保留等待复活。没有死亡能力也能运行，只是没有死亡动作，复活会在倒计时结束后最多再等 Death Finished Timeout。

## 3. 调参建议

| 想要 | 改什么 |
|---|---|
| 前几波更轻松 | 第一档 Base Count 调小、First Wave Delay 调大 |
| 后期压力更大 | 后面档位的 Count Per Wave、CurveTable 斜率；或缩短 Wave Interval |
| 怪物堆太多卡顿 | Max Alive Enemies 降低；Spawns Per Tick 降低 |
| 升级太快 / 太慢 | Experience To Next 曲线或线性公式；各档 Experience Multiplier；怪物 Experience Reward |
| 联机太简单 | Player Scaling 两条曲线斜率调大 |
| 阵亡惩罚太重 | Base Delay、Delay Per Death 调小；Revive Effect 加长无敌 |
| 怪物刷在脸上 | Min Distance To Players 调大；多摆刷怪器 |

运行中快速试数值用 CVar（只在开发构建生效，不写回资产）：`cc.Survival.WaveIntervalScale`、`cc.Survival.SpawnCountScale`、`cc.Survival.XPScale`。

## 4. 调试

### 4.1 控制台命令（在服务器 / 单机执行）

| 命令 | 作用 |
|---|---|
| `cc.Survival.Debug 1` | 屏幕显示阶段、波次、下一波倒计时、存活 / 队列数、等级经验、每个玩家的状态与复活倒计时 |
| `cc.Survival.SetWave 10` | 跳到第 10 波并立即开始 |
| `cc.Survival.SkipWave` | 立即开始下一波 |
| `cc.Survival.AddXP 100` | 全队加经验 |
| `cc.Survival.God` | 切换所有玩家的 `Status.DamageImmunity` |
| `cc.Survival.ReviveAll` | 立即复活所有阵亡玩家 |
| `cc.Survival.ClearEnemies` | 销毁所有波次怪物（不给经验） |
| `cc.Survival.EndMatch` | 立即结算 |

联机客户端上执行无效（会提示需要在服务器执行）；在主机上执行即可。

### 4.2 日志

`LogGAS_Demo`：`[Survival]` 阶段切换、每波档位 / 人数 / 计划数 / 入队数、升级、阵亡、复活、回收；`[Death]` 每次死亡的 Victim / Killer；`Verbose` 级别打印每只怪的生成参数。`LogCCBuff`：`[Revive]` 恢复了几个挂起的 Buff。

### 4.3 复现与平衡数据

- 开局日志会打印随机种子；用地图 URL `?SurvivalSeed=<种子>` 复现同一局的抽怪和落点顺序
- 开发构建每局在 `Saved/Survival/Survival_<时间>.csv` 写每波一行：波次、人数、刷出数、击杀、阵亡、用时、结束时存活怪数，可直接用表格软件画难度曲线
- 不同地图用不同难度：`?SurvivalConfig=/Game/.../DA_Hard.DA_Hard`

### 4.4 自动化测试

Session Frontend → Automation：`GAS_Demo.Survival`（档位查找、数量、升级、复活延迟、统计、生命耗尽、死亡广播）、`GAS_Demo.Buff.ResumeAfterRevive`、`GAS_Demo.Buff.Selection.ServerEvents`。

需要实机验证（单机 + 两个客户端的 PIE）：分帧生成与预警、刷怪器选择、复活落点与视角、选卡保护、结算页与再来一局、单机暂停。

## 5. 常见问题

| 现象 | 原因与处理 |
|---|---|
| 开局一直不刷怪 | ① GameMode 的 Config 没填（日志 Error）；② 没有勾选 Managed By Wave Director 的刷怪器（日志 Warning）；③ 对局没开始：Pawn 未就绪且 Start Timeout 未到 |
| 每波数量远少于 Base Count、怪物很脆 | Player Scaling 曲线在 1 人时不是 1：常见原因是 Count / Health Multiplier 的 **External Curve 引用了别的曲线**（如按波次的 0→20 曲线，1 人时约 0.15）。日志里的 `CountScale` / `HealthScale` 应为 1.00；清空 External Curve 或改为 (1,1) 起步。数据校验会对此给出警告 |
| 日志"入队 0" | 刷怪器附近找不到落点：检查地面碰撞、NavMesh、半径；或档位 Enemies 为空 |
| 放了多个刷怪器仍只生成 1 只 | 先看日志的"计划"数量。若计划就是 1，检查当前档 Base Count、Count Per Wave、Count Multiplier 在存活人数处的值，以及 cc.Survival.SpawnCountScale；若上限截断后变少，检查 Max Alive Enemies。受控刷怪器的 Spawn Count 不参与计算 |
| 怪物属性每波都一样 | 初始化 GE 用的是常数，改成 Scalable Float + CurveTable |
| 击杀不涨经验 | 怪物 Experience Reward 为 0；或不是玩家方击杀（怪物互杀、被回收不算） |
| 升级不弹三选一 | Progression.Buff Pool 为空；或池里的 GE 缺少 CC Buff UI Data（会被跳过） |
| 复活后升级 Buff 没了 | 该 GE 的 CC Buff Rules 没关 bRemoveOnDeath（2.3） |
| 复活后立刻又死 | Revive Effect 没给无敌，复活点被怪包围；加 `Status.DamageImmunity` 1～2 秒 |
| 阵亡后很久才复活 | 死亡动作没结束：玩家没有 `CC_Death`，或蒙太奇很长；见 2.9 与 Death Finished Timeout |
| 单人死亡没有结算页 | GameState 不是 `BP_SurvivalGameState` 或没设 Result Screen Class |
| 结算页按钮没反应 | 非房主无权限（bCanControlMatch 为 false）；单机"返回大厅"没有房间可回（日志警告） |
| HUD 没有生存数据 | `Get Survival` 的 bActive 为 false：地图的 GameState 不是 `CC_SurvivalGameState` |
| 联机选卡时被打死 | Choosing Protection Effect 没配 |

## 6. 本次改动带来的行为变化（所有模式）

| 改动 | 影响 |
|---|---|
| 死亡判定改为 `UCC_AttributeSet::OnOutOfHealth`（Health < 0.1，带击杀者） | `ACC_BaseCharacter::OnHealthChange` 不再调用 `HandleDeath`；死亡能力路径不变 |
| `HandleDeath` 幂等（已死亡直接返回） | 没有经过出生（`bAlive` 仍为初值 false）的角色调用它不会清理 |
| `CC_Death` 结束时上报 DeathFinished | 只是多一个事件，敌人仍在之后销毁 |
| `ACC_EnemySpawner` 拆出 `FindSpawnLocation / SpawnEnemyAt` | 原 `SpawnEnemies` 行为不变；落点改为先做胶囊重叠检查再生成 |
| `UCC_BuffSelectionComponent` 新增服务器事件 | 原有流程不变 |
| `UCC_CombatMainModel` 新增 `GetSurvival` | 非生存地图 bActive 为 false，原有 HUD 不受影响 |

## 7. 代码入口

| 想看 | 文件 |
|---|---|
| 规则、事件路由、击杀归属、全灭 | `Game/Survival/CC_SurvivalGameMode` |
| 波次、刷怪、回收、CSV | `Game/Survival/CC_WaveDirectorComponent` |
| 经验与升级 | `Game/Survival/CC_TeamProgressionComponent` |
| 复活 | `Game/Survival/CC_RespawnComponent`、`Actors/Character/CC_PlayerCharacter::Revive` |
| 配置与纯计算 | `Game/Survival/CC_SurvivalModeConfig` |
| 复制状态 | `Game/Survival/CC_SurvivalGameState`、`CC_SurvivalPlayerState` |
| 死亡事件 | `AbilitySystem/Attributes/CC_AttributeSet`（OnOutOfHealth）、`Actors/Character/CC_BaseCharacter`（HandleOutOfHealth）、`Subsystems/CC_GameEventSubsystem` |
| 调试命令与 CVar | `Private/Game/Survival/CC_SurvivalDebug.cpp` |
| 界面 | `UI/Combat/CC_CombatMainController`（GetSurvival）、`UI/Survival/CC_SurvivalResultController` |
| 测试 | `Private/Tests/CC_SurvivalTests.cpp` |
