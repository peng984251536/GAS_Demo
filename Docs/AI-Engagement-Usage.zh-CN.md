# 怪物包围寻路使用说明（槽位 + 令牌 + Detour Crowd）

设计原理、类图和时序图见 `Docs/AI-Engagement.zh-CN.md`。本文只讲怎么接入、怎么调、出问题怎么查。

## 1. 它做了什么

| 部分 | 效果 | 由谁负责 |
|---|---|---|
| 槽位 | 敌人围着目标分散站位：内圈攻击、外圈包围、弓兵站远程圈，站不下的在外侧排队 | `UCC_EngagementSubsystem`（自动） |
| 令牌 | 同一个目标同时只有少数几个敌人贴身攻击，出手自动错开 | `UCC_EngagementSubsystem`（自动发放），`ACC_AIController`（自动归还） |
| Detour Crowd | 去目标点的路上提前侧身绕开同伴和玩家 | `ACC_AIController` 的 `UCrowdFollowingComponent` |

行为树只需要**读黑板、按档位选分支、触发 `FollowTarget` 和攻击**。令牌的申请和归还都不需要行为树处理。

## 2. 接入清单

按顺序完成下面几步，C++ 部分已经就绪，剩下的都是编辑器里的资产配置。

### 2.1 编译

拉取分支后重新生成项目文件并编译。新增的源码都在 `Source/GAS_Demo/{Public,Private}/AI/` 下，不需要改 `Build.cs`（`AIModule`、`NavigationSystem`、`GameplayAbilities` 已在依赖里）。

### 2.2 项目设置：Crowd Manager

**Project Settings → Engine → Crowd Manager**：

| 设置 | 建议值 | 说明 |
|---|---|---|
| Max Agents | 敌人上限 + 余量，例如 200 | 默认 50，超出的敌人不会参与避障 |
| Max Agent Radius | 大于最大胶囊体半径，例如 80 | 太小时大体型敌人注册失败 |

关卡里需要有 NavMesh（原来的寻路已经依赖它）。

### 2.3 黑板资产：新增 3 个键

在敌人行为树使用的黑板里新增（**键名必须完全一致**）：

| 键名 | 类型 | 说明 |
|---|---|---|
| `EngagementTier` | Enum，Enum Type 选 `ECC_EngagementTier` | `None` / `Queue` / `Surround` / `Engaged` |
| `MoveLocation` | Vector | 应该走到的位置：槽位或排队点；无效时被清空 |
| `HasAttackToken` | Bool | 是否持有攻击令牌，等价于 `EngagementTier == Engaged` |

原有的 `TargetActor`、`LastKnownLocation` 保持不变。键不存在时控制器写入会静默失败，敌人会表现为「有目标但不动」。

### 2.4 行为树：按档位分支

推荐结构（只示意战斗部分，巡逻 / 搜索沿用原来的）：

```
Selector（战斗，条件：TargetActor 已设置）
├─ [EngagementTier == Engaged] Sequence                       ← 装饰器 Observer Aborts: Both
│     ├─ [MoveLocation 已设置，Notify Observer: On Value Change，Observer Aborts: Self]
│     │     发送事件 CCTags.CCAbilityTrigger.FollowTarget，等待能力结束
│     └─ 攻击（原有的 CC_AttackIntend 任务）
├─ [EngagementTier == Surround] Sequence                      ← Observer Aborts: Both
│     ├─ [MoveLocation 同上] 发送事件 FollowTarget
│     ├─ 弓兵：攻击（射箭）；近战：Wait 0.5～1 秒（在外圈游走 / 威胁）
│     └─ 弓兵过近时仍可用原有的 KeepDistance 后撤
└─ [EngagementTier == Queue] Sequence
      ├─ 发送事件 FollowTarget（走到排队点；已在外侧时会立即完成）
      └─ Wait 0.5 秒（避免空转）
```

要点：

- **所有档位的移动都用 `FollowTarget`**。它会读取子系统分配的 `MoveLocation`，没有分配时才回退到原来的环绕选点逻辑
- **`MoveLocation` 变化时要重新触发 `FollowTarget`**：给移动节点加一个监听 `MoveLocation` 的黑板装饰器，Notify Observer 选 `On Value Change`、Observer Aborts 选 `Self`。`FollowTarget` 已允许重新触发，旧的移动任务会被正确清理。子系统只在位置移动超过 75 厘米时才更新这个键，不会频繁重启
- **`EngagementTier` 装饰器用 `Observer Aborts: Both`**，这样拿到令牌时会立刻从 Surround 分支切到 Engaged 分支
- **不需要申请或归还令牌**。子系统每 0.25 秒自动把令牌发给合适的近战，攻击能力结束时控制器自动归还
- Engaged 到达内圈槽位后应尽快出手：**2 秒内既没靠近也没开始攻击，令牌会被收回**

### 2.5 攻击能力：确认会授予 `CCAbilities.Attack`

`UCC_Primary`（近战）和 `UCC_BowShoot`（射箭）的构造函数已经在 `ActivationOwnedTags` 里加了 `CCTags.CCAbilities.Attack`。如果你的近战攻击是这两个类的**蓝图子类**，打开蓝图的 Class Defaults，确认 `Activation Owned Tags` 里仍有这个标签（蓝图里改过这个字段时会覆盖 C++ 默认值）。

**以后新增任何攻击能力**（重击、技能等），都要在构造函数里加：

```cpp
ActivationOwnedTags.AddTag(CCTags::CCAbilities::Attack);
```

否则令牌不会在攻击结束时归还（只能等 2 秒超时被收回），攻击期间也不会切到 `ObstacleOnly`，会被路过的同伴推着走。

### 2.6 不需要额外配置的部分

- **控制器**：`ACC_AIController` 构造时已把寻路组件换成 `UCrowdFollowingComponent`，蓝图子类会自动继承
- **玩家**：`ACC_PlayerCharacter` 已挂 `CrowdAgent` 组件，服务器上自动注册为 Crowd 障碍
- **兵种**：角色配置里设置了 `ArrowProjectileClass` 的敌人自动按弓兵处理（站远程圈、不用令牌）
- **魅惑**：被魅惑后换目标会自动转到新目标的围攻组，旧组的令牌自动归还

## 3. 调参

### 3.1 全局参数（DefaultGame.ini）

在 `Config/DefaultGame.ini` 中添加，只写需要改的字段，其他字段保持默认：

```ini
[/Script/GAS_Demo.CC_EngagementSubsystem]
Settings=(TokenCapacity=4,InnerRadius=180.0,OuterRadius=450.0,SlotSpacing=120.0,MinGrantInterval=0.4)
```

| 参数 | 默认值 | 作用 | 怎么调 |
|---|---|---|---|
| `TokenCapacity` | 4 | 同一目标同时贴身攻击的人数上限 | 玩家觉得被围殴就调小 |
| `MinGrantInterval` | 0.4 秒 | 两次发令牌的最小间隔 | 想让出手更错开就调大 |
| `InnerRadius` | 180 | 内圈半径，约等于近战攻击距离 + 胶囊体半径 | 敌人到位后打不到人就调小 |
| `OuterRadius` | 450 | 外圈半径 | 想要更强压迫感就调小，但要给内圈进出留空间 |
| `SlotSpacing` | 120 | 槽位间距，至少 2 × 胶囊体半径 + 40 | 相邻敌人互相推挤抖动就调大 |
| `SlotJitter` | 15 | 槽位随机偏移，避免站成完美的圆 | |
| `QueueMargin` | 100 | 排队点在所在环外侧的距离 | |
| `RangedRadiusFallback` | 650 | 弓兵配置缺失时远程圈半径 | 正常情况取弓兵射程中线 |
| `StuckTimeout` | 2 秒 | 持令牌者无进展多久收回令牌 | 地形复杂、经常误收回就调大 |
| `SlotCooldown` | 3 秒 | 真正卡住时该内圈槽位的冷却 | |
| `TargetFastSpeed` | 300 | 目标快于这个速度时，追击者不算卡住 | 略低于玩家跑步速度 |
| `StuckSelfSpeed` | 50 | 敌人慢于这个速度才算「真的卡住」 | |
| `ArriveRadius` | 60 | 离内圈槽位多近算到达 | |
| `UpdateInterval` | 0.25 秒 | 每个围攻组的更新间隔 | 敌人多、性能紧张时调大 |
| `NotifyDistance` | 75 | 分配位置移动多远才更新黑板 | 敌人频繁重新寻路就调大 |

### 3.2 每个敌人的参数

| 位置 | 参数 | 作用 |
|---|---|---|
| 角色配置 `UCC_CharacterConfig` | `AttackDelay` | 归还令牌后多久才能再次被选中（每个敌人的出手冷却） |
| 角色配置 | `MinRange` / `MaxRange` | 弓兵的远程圈半径取两者中线 |
| `CC_FollowTarget` 的蓝图默认值 | `EngagedAcceptanceRadius`（30） | 走向内圈槽位的可接受半径 |
| `CC_FollowTarget` 的蓝图默认值 | `DefaultAcceptanceRadius`（50） | 走向外圈、远程圈、排队点的可接受半径 |

### 3.3 避障参数

在 `UCC_CrowdAgentStatics::ConfigureCrowdAgent` 中：分离开启、分离权重 2、转弯预判开启、邻居查询范围 600。避障质量按档位自动切换：排队 Low、包围 Medium、交战 High。

## 4. 调试

### 4.1 可视化

运行时打开控制台输入：

```
cc.Engagement.Debug 1
```

每个被围攻目标周围会画出三圈槽位：

| 颜色 | 含义 |
|---|---|
| 绿 | 空闲可用 |
| 橙 | 内圈已被持令牌者占用 |
| 黄 | 外圈已被包围者占用 |
| 青 | 远程圈已被弓兵占用 |
| 红 | 不可用：投影不到 NavMesh、被墙挡住、或在冷却中 |

持令牌者到它的内圈槽位之间有一条橙色连线，目标头顶显示 `Tokens 已用/容量  Attackers 人数`。

### 4.2 日志

所有日志都在 `LogGAS_Demo` 下，前缀 `[AI][Engagement]` 和 `[AI][Crowd]`。查看详细日志：

```
log LogGAS_Demo Verbose
```

| 日志 | 含义 |
|---|---|
| `Register` / `Unregister` | 加入 / 离开某个目标的围攻组 |
| `Token granted` | 发放令牌，带槽位和编号 |
| `Token released` | 攻击结束后正常归还 |
| `Release ignored` | 编号对不上或没有令牌（弓兵射击、换目标后旧攻击结束），属正常（Verbose） |
| `No progress, revoke` | 进展检查超时收回令牌，`CooldownSlot=1` 表示判定为真的卡住 |
| `Pruned` | 攻击者死亡、目标死亡或不再敌对，被清理出组 |
| `Assignment` | 控制器收到新分配（Verbose） |

### 4.3 自动化测试

**Tools → Session Frontend → Automation**，筛选 `GAS_Demo.AI.Engagement`：

- `SlotRing`：槽位数、增量分配的保留 / 补位 / 夹角重分配 / 降档、不可用槽位、冷却
- `TokenPool`：容量、幂等、归还、消耗值、清理

## 5. 常见问题

**敌人有目标但站着不动**

1. 黑板是否有 `EngagementTier`、`MoveLocation`、`HasAttackToken` 三个键，名字是否完全一致
2. 行为树是否按档位触发了 `FollowTarget`
3. 打开 `log LogGAS_Demo Verbose`，看有没有 `[AI][Engagement] Assignment` 日志：没有说明没注册上，检查控制器是否是 `ACC_AIController` 或其子类

**敌人一直在外圈，从不上来打**

1. 打开 `cc.Engagement.Debug 1`，看内圈是否全红：玩家附近没有 NavMesh，或玩家贴着墙
2. 看有没有 `Token granted` 日志：没有的话检查 `AttackDelay` 是否过大
3. 有发放但很快 `No progress, revoke`：说明到达后没在 2 秒内出手，检查行为树 Engaged 分支是否在 `FollowTarget` 后接了攻击

**令牌发出去后一直不还（`Tokens 4/4` 不变）**

攻击能力没有授予 `CCAbilities.Attack`，见 2.5。此时只能靠 2 秒超时收回，节奏会很拖沓。

**相邻敌人互相推挤、原地抖动**

调大 `SlotSpacing`；确认 Crowd Manager 的 `Max Agents` 够用（超出上限的敌人不参与避障，会直接顶人）。

**攻击时被同伴推着滑走**

攻击能力没有授予 `Attack` 标签，没有切到 `ObstacleOnly`，见 2.5。

**日志里有 Crowd 状态切换失败的警告**

引擎只允许在路径跟随空闲时切换 Crowd 模拟状态，攻击开始时已经先停止移动。偶发一条不影响功能，持续出现说明攻击开始时敌人还在被别的逻辑驱动移动。

## 6. 本次改动带来的行为变化

| 变化 | 影响 | 需要你决定吗 |
|---|---|---|
| `UCC_Primary` 授予 `Attack` 标签 | `CC_Dash` 原本配置的「攻击中不能冲刺」开始生效（玩家的近战也会授予这个标签） | **是**：想让玩家能用冲刺取消攻击，删掉 `CC_Dash` 构造函数里的 `ActivationBlockedTags.AddTag(CCTags::CCAbilities::Attack)` |
| `UCC_BowShoot` 授予 `Attack` 标签 | `CC_KeepDistance` 原本配置的「射箭时不后撤」开始生效 | 否，这是原本的设计意图 |
| 箭对 Pawn 改为重叠检测 | 箭穿过友方（包括包围圈里的同伴），只在命中敌对目标或世界障碍时销毁 | 否 |
| `CC_FollowTarget` 允许重新触发 | 移动途中再次触发会按新位置重新寻路，而不是被忽略 | 否 |
| 敌人寻路组件换成 Crowd | 敌人之间会提前避让；不要再开启 CharacterMovement 的 RVO | 否 |

不受影响：`CC_AttackIntend`、`CC_KeepDistance` 的逻辑、`CC_FindRangedTarget`、`CC_FindPlayerTarget`。

## 7. 代码入口

| 文件 | 内容 |
|---|---|
| `AI/Engagement/CC_EngagementSubsystem` | 唯一对外入口：`RegisterAttacker` / `UnregisterAttacker` / `ReleaseToken` / `GetAssignment` / `OnAssignmentChanged` |
| `AI/Engagement/CC_EngagementTypes.h` | `ECC_EngagementTier`、`FCC_EngagementSettings`、`FCC_EngagementAssignment` |
| `AI/Engagement/CC_EngagementGroup.h` | 围攻组与攻击者条目（内部数据） |
| `AI/Engagement/Slots/CC_SlotRing` | 槽位环：生成与增量分配（纯逻辑） |
| `AI/Engagement/Tokens/CC_AttackTokenPool` | 令牌池（纯逻辑） |
| `AI/Crowd/CC_CrowdAgentStatics` | 避障参数、按档位调质量、攻击时切 `ObstacleOnly` |
| `AI/Crowd/CC_PlayerCrowdAgentComponent` | 玩家注册为 Crowd 障碍 |
| `Player/CC_AIController` | 注册 / 注销、写黑板、监听 `Attack` 标签 |
| `AbilitySystem/Abilities/Logic/CC_FollowTarget` | 走到分配位置 |

模块依赖规则：`Slots` 和 `Tokens` 互不依赖，也不依赖任何 Actor；`Crowd` 只依赖档位枚举，不知道槽位和令牌；能力不依赖 `Crowd`。
