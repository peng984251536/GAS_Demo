> **已被 v3 取代（2026-10-20）**：槽位 + 令牌方案已删除，现行设计与接入步骤见 `Docs/AI/AI-Combat.zh-CN.md`。本文仅作历史记录。

# 怪物包围寻路设计（槽位 + 令牌 + Detour Crowd）v2.2

> 已按 2026-10-20 当前工作区源码核对类名、成员和调用流程。接入与调参步骤见 [使用说明](AI-Engagement-Usage.zh-CN.md)。第 13 节记录实现细节与边界。
> 目标规模：同屏 100～150 只敌人（近战为主，含弓兵）。
> 所有图均为 PlantUML，可直接粘贴到语雀的「文本绘图 → PlantUML」块中。

## 0. 修改记录

实现原则：**只修会导致错误行为的漏洞，不新增类和子系统**。本次 v2.2 仅同步文档，没有修改运行时代码或行为树资产。

### v2.2：按当前源码同步设计图

- 类图补齐实际函数名、参数、返回值、可见性和成员类型；图中省略的是与围攻无关的成员，不使用虚构函数表示实际调用。
- 区分子系统委托 `OnAssignmentChanged`、控制器回调 `OnEngagementAssignmentChanged` 和订阅句柄 `AssignmentChangedHandle`。
- 换目标时由 `RegisterAttacker` 内部调用 `RemoveAttacker(Key, false)` 静默移出旧组；周期更新通过 `NotifyIfChanged` 排队通知，再由 `FlushNotifies` 广播。
- 同步令牌归还、死亡清理、编号校验、Crowd 注册对象和移动任务调用链；明确行为树结构仍需按使用说明配置。

### v2：设计逻辑漏洞

| # | 漏洞 | 修法 |
|---|---|---|
| 1 | 先发令牌，下一次更新才分内圈槽位，贴墙时拿到令牌却没地方站 | 发令牌与占内圈槽位为同一步，没有可用槽位就不发 |
| 2 | 归还只认攻击者，换目标后旧攻击结束会误放新令牌 | 每次发令牌生成分配编号，归还必须带编号 |
| 3 | 卡住的怪一直占着令牌和槽位 | 周期更新中做进展检查，超时收回 |
| 4 | 全局按总距离重排，玩家一动怪物就换边 | 增量分配：有效旧槽位保留，只给需要的人补位 |
| 5 | 令牌容量固定，贴墙时回到漏洞 1 | 同时检查令牌余量和空闲可用内圈槽位；两者都有才发放 |
| 6 | 令牌只限人数，几只怪可能同一帧出手 | 同一目标两次发令牌至少间隔 `MinGrantInterval`；错开发放不等于保证实际攻击起手错开 |
| 7 | 档位枚举混入兵种 `Ranged` | 枚举删除 `Ranged`，兵种用 `bRanged` |

### v2.1：与现有代码对接时发现的问题

| # | 问题 | 修法 |
|---|---|---|
| 8 | 没有任何能力授予 `CCAbilities.Attack`，依赖它的 `ObstacleOnly` 切换和进展检查都不生效 | 近战攻击能力 `UCC_Primary` 和 `UCC_BowShoot` 构造函数中 `ActivationOwnedTags` 加上它 |
| 9 | `CC_AttackIntend` 只是派发者，在它结束时归还令牌时机不对 | 控制器监听 `Attack` 标签：0→1 记下编号，1→0 带编号归还。`CC_AttackIntend` 不改 |
| 10 | `CC_KeepDistance` 在 `MinRange <= 0` 时直接失败，且只在过近时后撤，不能做近战外圈移动 | 所有档位的「走到分配位置」统一用 `CC_FollowTarget`；`CC_KeepDistance` 只保留给弓兵后撤，不改 |
| 11 | `CC_FollowTarget` 不允许重新触发，移动途中收不到新槽位 | 改为 `bRetriggerInstancedAbility = true` |
| 12 | 行为树主动申请令牌，谁先 Tick 谁先拿，会让对面的怪穿过包围圈 | 子系统在周期更新中主动发放给最合适的人；删除对外的 `RequestToken` |
| 13 | 玩家逃跑时，追击者离槽位不缩短，被误判为卡住 | 目标快速移动时不计时；只有自身几乎静止时才冷却槽位 |
| 14 | 只要槽位有效就保留，玩家跑过去后怪要绕到另一侧 | 怪的方位与槽位方位夹角超过 90° 视为失效 |
| 15 | 薄墙另一侧的槽位能投影成功，但要绕很远 | 再从目标向槽位做一次 `NavigationRaycast`，被挡则不可用 |
| 16 | 箭被包围圈里的近战友军挡住销毁 | 箭对 Pawn 改为重叠检测，用 `IsHostile` 判断，友方穿过 |
| 17 | 排队位置描述有歧义 | 沿自身方位停在外圈半径 + `QueueMargin`；已在更远处就原地等 |
| 18 | 网络归属未说明 | 子系统在 Game / PIE World 创建，正常由服务器 AIController 注册攻击者；玩家 Crowd Agent 只在服务器注册 |
| 19 | 持令牌到达槽位后若一直不出手，会永久占用令牌 | 进展检查中「到达但未攻击」同样计时，超时收回 |

**当前冲刺行为**：`UCC_Dash` 的 `ActivationBlockedTags` 包含 `CCTags::CCAbilities::Attack`，攻击状态存在时不能激活冲刺。移除阻断只表示允许激活冲刺，并不自动取消正在执行的攻击能力；攻击取消策略需要另行定义。

**本版不做**（属于后续改进）：攻击后的「恢复」阶段、跨组槽位冲突检查、弓兵齐射预算、能力主动上报执行结果。

## 1. 目标与范围

| 要解决的问题 | 负责部分 |
|---|---|
| 所有敌人都追同一个点，围成一团 | 槽位 |
| 太多敌人同时贴身攻击，玩家读不懂局面，开销也大 | 令牌 |
| 去目标点的路上互相顶住、滑开 | Detour Crowd |
| 上百只时开销失控 | 按档位降级 |

**不在本设计范围内**：感知层的 N² 问题、动画预算、网络复制优化。

## 2. 文件夹与模块

```
Public/AI/
├── CC_AIController.h             感知、目标选择、围攻与避障接入
├── Engagement/                    决策层：谁去哪、谁能打
│   ├── CC_EngagementTypes.h       共享类型：档位枚举、分配结果、配置
│   ├── CC_EngagementSubsystem.h   协调者：唯一对外入口，组合槽位与令牌
│   ├── CC_EngagementGroup.h       单个目标的围攻组（内含槽位环 + 令牌池）
│   ├── Slots/
│   │   └── CC_SlotRing.h          纯逻辑：生成环形槽位、增量分配
│   └── Tokens/
│       └── CC_AttackTokenPool.h   纯逻辑：令牌发放、回收
└── Crowd/                         避障层：路上怎么不撞
    ├── CC_CrowdAgentStatics.h     按档位切换避障质量、攻击时切 ObstacleOnly
    └── CC_PlayerCrowdAgentComponent.h  让玩家注册为 Crowd Agent 的组件
（Private/AI/ 下同结构放 .cpp）
```

分两个模块而不是三个平级文件夹：槽位和令牌需要一个协调者（发令牌必须同时占内圈槽位），所以放在同一个 `Engagement` 模块下作为互不依赖的纯逻辑子目录；Crowd 是另一层，只看档位，不知道槽位和令牌。

```plantuml
@startuml 模块依赖
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
skinparam packageStyle rectangle

package "AI/Engagement/Slots" as Slots
package "AI/Engagement/Tokens" as Tokens
package "AI/Engagement\n(Subsystem / Group / Types)" as Eng
package "AI/Crowd" as Crowd
package "Utils/Team\nUCC_TeamStatics" as Team
package "AI\nACC_AIController" as AIC
package "AbilitySystem/Abilities/Logic\nCC_FollowTarget" as GA
package "UE AIModule\nUCrowdFollowingComponent\nUNavigationSystemV1" as UE

Eng --> Slots
Eng --> Tokens
Eng --> Team : IsHostile 过滤
Eng --> UE : 投影 + 射线检查槽位
Crowd --> Eng : 只用 Types 里的档位枚举
Crowd --> UE
AIC --> Eng : 注册 / 注销 / 带编号归还
AIC --> Crowd : 档位变化、攻击标签变化
GA --> Eng : 只读分配结果

note bottom of Slots
  Slots 与 Tokens 互不依赖，
  也不依赖任何 Actor / ASC，
  可单独写自动化测试
end note
note bottom of Crowd
  不依赖 Slots / Tokens；
  更换移动方案时仍需评估
  Controller / MoveTo 的适配
end note
@enduml
```

禁止的依赖：`Slots` ↔ `Tokens`、`Crowd` → `Slots/Tokens`、`Slots/Tokens` → 任何 Actor 或能力类、能力 → `Crowd`。攻击能力不依赖 `Engagement`，它们只负责授予 `Attack` 标签。

## 3. 总体架构

```plantuml
@startuml 包围寻路总体架构
skinparam componentStyle rectangle
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"

package "已有：感知与选目标" {
  [ACC_AIController\nApplyTarget\n监听 Attack 标签] as AIC
}

package "决策层 AI/Engagement（正常由服务器 AI 使用）" {
  [UCC_EngagementSubsystem\n唯一对外入口\n0.25 秒更新\n发放令牌 + 进展检查] as SUB
  [FCC_EngagementGroup\n每个被围攻目标一个] as GRP
  [FCC_SlotRing\n内圈 / 外圈 / 远程圈] as RING
  [FCC_AttackTokenPool\n令牌池] as POOL
}

package "避障层 AI/Crowd" {
  [UCrowdFollowingComponent\n(引擎)] as CFC
  [UCC_CrowdAgentStatics\n按档位调质量\n攻击时 ObstacleOnly] as CST
  [UCC_PlayerCrowdAgentComponent\n玩家注册为 Agent] as PCA
}

package "已有：行为与能力" {
  [行为树\n读黑板档位与令牌] as BT
  [CC_FollowTarget\n走到分配位置（所有档位）] as FT
  [UCC_AttackIntend\n可选攻击意图入口] as ATK
  [UActionComponent\nTryAttack] as ACTION
  [UAbilitySystemComponent\nGameplayEvent / 标签事件] as ASC
  [UCC_Primary / UCC_BowShoot\n授予 Attack 标签] as MECH
  [UAITask_MoveTo] as MOVE
  [UCrowdManager] as CM
}

AIC <-right-> SUB : ① 注册 / 注销、带编号归还\n② 分配变化回调
SUB -down-> GRP
GRP -down-> RING
GRP -down-> POOL
AIC -down-> BT : ③ 写黑板\n档位 / 位置 / 令牌
AIC -left-> CST : 档位变化\nAttack 标签变化
BT -down-> FT
BT -down-> ATK : 可选
BT --> ACTION : 现有攻击 Task 也可直接调用
ATK -down-> ACTION : TryAttack(ExecutionTag)
ACTION --> ASC : HandleGameplayEvent
ASC --> MECH : 激活实际攻击能力
MECH --> ASC : ActivationOwnedTags\n激活期间持有 Attack
ASC .up.> AIC : OnAttackTagChanged(Tag, NewCount)
FT .up.> SUB : 读分配
FT -down-> MOVE
MOVE -down-> CFC : 路径跟随 + 避障
CST -down-> CFC
PCA -down-> CM : RegisterAgent(this)\n玩家组件作为障碍
CFC --> CM : AI agent 的路径跟随与避障
@enduml
```

**一句话分工**：控制器决定「打谁」（已有）并负责与子系统对接；子系统决定「站哪、谁能打」并检查进展；行为树决定「现在做什么」；能力只负责执行一次移动或攻击；Crowd 负责「路上别撞」。

**状态归属**：

- 子系统是槽位和令牌的唯一真实来源
- 黑板只是子系统推送过来的副本，供行为树读取
- 控制器是唯一会改动围攻状态的外部调用方（注册、注销、归还）
- 能力完全不维护围攻状态：`CC_FollowTarget` 只读分配，攻击能力只授予标签

**为什么用 UTickableWorldSubsystem**：每个 Game / PIE World 有自己的管理器实例，由引擎创建并调用 `Tick`，不是进程全局单例。被围攻的不一定是玩家；子系统按目标懒创建围攻组，玩家、随从、被魅惑的敌人共用一套逻辑。组内没有攻击者时移除该组，不销毁目标 Actor。空组时 `Tick` 直接返回。

## 4. 核心规则

### 4.1 档位与兵种

档位只表示**参与程度**，兵种用 `bRanged` 单独表示。所有档位的移动都由 `CC_FollowTarget` 走到「分配位置」完成。

| 档位 `ECC_EngagementTier` | 近战的分配位置 | 弓兵（`bRanged`）的分配位置 | 避障质量 |
|---|---|---|---|
| `None` | 未注册或已退出围攻组 | 未注册或已退出围攻组 | Low |
| `Queue` | 外圈之外的排队点 | 远程圈之外的排队点 | Low |
| `Surround` | 外圈槽位 | 远程圈槽位，在此射击 | Medium |
| `Engaged` | 内圈槽位，持有令牌 | 本版不使用 | High |

弓兵过近时的后撤仍由现有的 `CC_KeepDistance` 负责，不改。

### 4.2 槽位环

- `FCC_SlotRing::ComputeSlotCount` 计算槽位数：`max(1, floor(2π × Radius / Spacing))`；半径或间距过小时返回 1。槽位间距建议至少为「2 × 最大胶囊体半径 + 分离余量」，代码不会自动读取胶囊尺寸强制这个下限。
- 槽位偏移是**世界方向**的，只随目标平移、不随目标朝向旋转，玩家转身时槽位不动；每个槽位加一个固定的小随机偏移，避免站成完美的圆
- **有导航系统时的槽位校验**由 `RebuildRings` 调用 `ValidateRing` 完成（v2.1 #15）：
  1. 投影到 NavMesh 成功
  2. 目标自身投影成功时，从投影后的目标位置到槽位做 `NavigationRaycast` 没有被挡
  3. 不在冷却中
- 贴墙时只剩半圆或一段弧是正常结果，多出来的人进入 `Queue`

当前实现没有导航系统时跳过 `ValidateRing`，保留重建后的原始位置；目标自身投影失败时跳过射线检查。这些位置并不因此保证可达，移动任务仍可能失败。

**增量分配**：

1. 满足以下全部条件的人保留旧槽位：旧槽位仍在本环、可用、未被占，且**怪相对目标的方位与槽位方位夹角不超过 90°**（v2.1 #14）
2. 其余的人按离目标由近到远，各自挑离自己最近的空闲可用槽位
3. 没有空位时输出 `INDEX_NONE`；`AssignSlots` 对外圈 / 远程圈成员降为 `Queue`，对内圈持有者调用 `RevokeToken`，再尝试放回外圈，没有外圈位置才进入 `Queue`

**排队位置**（v2.1 #17）：沿怪自己相对目标的方位，停在所在环半径 + `QueueMargin`（默认 100）处；已经在这个距离之外的原地等待，不移动。

### 4.3 令牌（子系统主动发放）

**发放**（v2.1 #12）：子系统在每个组的周期更新末尾，最多发放一个令牌。条件：

1. 距离本组上次发放 ≥ `MinGrantInterval`（默认 0.4 秒）
2. 对候选人满足 `Group.Tokens.Available() >= Entry.TokenCost`，并能由 `InnerRing.FindFreeSlotNear` 找到一个空闲可用槽位；不是比较「持有数 < 空闲槽位数」
3. 候选人：近战、`Surround` 档、Actor 有效、不在攻击中、当前时间 ≥ 自己的 `NextEligibleTime`；存活和敌对关系先由 `PruneInvalid` 检查

满足时，从候选人中选**离最近空闲内圈槽位最近**的那个，一步完成：`Tokens.TryAcquire(Key, TokenCost)`、释放原外圈槽位、占内圈槽位、档位改为 `Engaged`、通过 `AllocateGeneration()` 分配新编号、记录 `LastGrantTime`。当前 `TokenCost` 默认 1；没有按等待时长或屏幕可见性排序。行为树读取黑板，不调用申请令牌接口。

`MinGrantInterval` 限制的是发放时刻；移动耗时不同仍可能导致多只怪同时起手。子系统也没有在实际攻击能力入口强制检查令牌，近战行为树必须在攻击前检查资格。

**归还**（v2.1 #9）：由控制器监听自己 ASC 上的 `CCAbilities.Attack` 标签。

- `BindEngagementEvents` 使用 `NewOrRemoved` 监听标签从 0 到非零、从非零到 0 的变化；不监听非零计数之间的变化
- 从 0 到非零（开始攻击）：`OnAttackTagChanged` 记下当前分配编号 `HeldGeneration`，调用 `SetAttacking(this, true)`
- 从非零到 0（所有攻击状态来源结束）：先调用 `SetAttacking(this, false)` 恢复 Crowd，再调用 `ReleaseToken(BaseCharacter, HeldGeneration)`
- 子系统只在「档位为 Engaged、确实持有令牌、编号一致」时处理：释放内圈槽位和令牌、分配新编号、设置 `NextEligibleTime = 现在 + AttackDelay`，再由 `PlaceInSurroundRing` 放回外圈；没有空位时进入 `Queue`。否则忽略

一张令牌持续到攻击标签计数归零。如果连段交接时始终有能力持有标签，就可以覆盖整套连段；如果交接中计数降到 0，就会触发归还，不能仅凭「连段」假定标签一直存在。弓兵仍会切换 Crowd，但其归还调用因未持有令牌而被忽略。`ActivationOwnedTags` 才会向 ASC 授予状态，仅设置 Ability Asset Tags 不会触发这种通知。

**其他回收路径**：目标切换由 `RegisterAttacker` 内部静默移出旧组；无目标或解除附身调用 `UnregisterAttacker`；死亡等失效情况由 `PruneInvalid` 兜底；超时由 `RevokeToken` 收回。归还 / 收回时重新分配编号，移除条目时直接丢弃编号，退出通知使用默认分配（`Generation = 0`）。编号是子系统级 `int32` 序列，不保证同一怪的新编号恰好等于旧编号 +1，达到 `MAX_int32` 后回到 1。

### 4.4 进展检查

只对 `Engaged` 的攻击者做，在周期更新中执行，不需要能力上报。每次更新：

| 情况 | 处理 |
|---|---|
| 正在攻击（带 `Attack` 标签） | 计时清零 |
| 离内圈槽位的距离比最近记录短 25 厘米以上 | 更新最近记录，计时清零 |
| 未到达槽位，且目标移动速度 > `TargetFastSpeed`（默认 300） | 不计时（v2.1 #13，追逃不算卡住） |
| 其他情况，包括已到达但还没出手 | 累计计时（v2.1 #19） |

累计达到 `StuckTimeout`（默认 2 秒）时调用 `RevokeToken`，分配新编号并尝试返回外圈，没有位置时进入 `Queue`。只有在**未到达槽位且自身速度 < `StuckSelfSpeed`（默认 50）**时，才把这个内圈槽位冷却 `Settings.SlotCooldown`（默认 3 秒）；其他情况只收回令牌，不惩罚槽位。`AssignSlots` 还会处理内圈槽位失效：先尝试换位，没有可分配的内圈槽位才收回令牌。

### 4.5 网络

- 只有服务器上的 `ACC_AIController` 会注册攻击者，客户端上的子系统实例没有任何组（实现调整见第 13 节）
- `UCC_PlayerCrowdAgentComponent` 只在服务器上注册到 `UCrowdManager`
- 黑板和行为树本来就只在服务器运行，不受影响

## 5. 类图

以下成员与当前头文件对应；为可读性省略 `UPROPERTY`、`UFUNCTION`、部分 `const` / 引用修饰及与围攻无关的成员。`+` 表示 public，`#` 表示 protected，`-` 表示 private。

```plantuml
@startuml 包围寻路类图
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
hide empty members

package "AI/Engagement" {
  enum ECC_EngagementTier {
    None
    Queue
    Surround
    Engaged
  }

  class FCC_EngagementSettings <<struct>> {
    +float InnerRadius = 180
    +float OuterRadius = 450
    +float SlotSpacing = 120
    +float SlotJitter = 15
    +float RangedRadiusFallback = 650
    +float QueueMargin = 100
    +FVector ProjectExtent = (100, 100, 250)
    +int32 TokenCapacity = 4
    +float MinGrantInterval = 0.4
    +float StuckTimeout = 2.0
    +float SlotCooldown = 3.0
    +float TargetFastSpeed = 300
    +float StuckSelfSpeed = 50
    +float ProgressStep = 25
    +float ArriveRadius = 60
    +float UpdateInterval = 0.25
    +float NotifyDistance = 75
  }

  class FCC_EngagementAssignment <<struct>> {
    +ECC_EngagementTier Tier
    +FVector MoveLocation
    +bool bHasToken
    +bool bLocationValid
    +int32 Generation
  }

  class UCC_EngagementSubsystem <<UTickableWorldSubsystem>> {
    -FCC_EngagementSettings Settings
    -TMap<FObjectKey, FCC_EngagementGroup> Groups
    -TMap<FObjectKey, FObjectKey> AttackerToTarget
    -TArray<TPair<TWeakObjectPtr<AActor>, FCC_EngagementAssignment>> PendingNotifies
    -int32 NextGeneration = 1
    +ShouldCreateSubsystem(Outer) : bool
    +Deinitialize() : void
    +GetStatId() : TStatId
    +RegisterAttacker(Attacker, Target, bRanged)
    +UnregisterAttacker(Attacker)
    +GetAssignment(Attacker) : FCC_EngagementAssignment
    +ReleaseToken(Attacker, Generation : int32) : void
    +GetSettings() : FCC_EngagementSettings
    +FCC_OnEngagementAssignmentChanged OnAssignmentChanged
    +Tick(DeltaTime) : void
    -GetNow() : float
    -AllocateGeneration() : int32
    -FindGroupOf(AttackerKey) : FCC_EngagementGroup*
    -RemoveAttacker(AttackerKey, bNotify) : void
    -UpdateGroup(Group, GroupDeltaTime) : void
    -PruneInvalid(Group) : void
    -RebuildRings(Group, Now) : void
    -ValidateRing(Ring, NavCenter, bHasNavCenter) : void
    -CheckProgress(Group, GroupDeltaTime, Now) : void
    -AssignSlots(Group, Now) : void
    -TryGrantToken(Group, Now) : void
    -RevokeToken(Group, Entry, bCooldownSlot, Now) : void
    -PlaceInSurroundRing(Group, Entry) : void
    -BuildAssignment(Group, Entry) : FCC_EngagementAssignment
    -NotifyIfChanged(Group, Entry, bForce) : void
    -FlushNotifies() : void
    -ComputeRangedRadius(Attacker) : float
    -{static} GetAttackDelay(Attacker) : float
    -{static} IsAttacking(Attacker) : bool
    -DrawDebug(Group) : void
  }

  class FCC_EngagementGroup <<struct>> {
    +TWeakObjectPtr<AActor> Target
    +FCC_SlotRing InnerRing
    +FCC_SlotRing OuterRing
    +FCC_SlotRing RangedRing
    +float RangedRadius
    +FCC_AttackTokenPool Tokens
    +TArray<FCC_AttackerEntry> Attackers
    +float LastGrantTime
    +float TimeSinceUpdate
    +bool bForceUpdate
    +FindEntry(Key) : FCC_AttackerEntry*
    +RingFor(Entry) : FCC_SlotRing
  }

  class FCC_AttackerEntry <<struct>> {
    +TWeakObjectPtr<AActor> Attacker
    +FObjectKey Key
    +ECC_EngagementTier Tier
    +bool bRanged
    +int32 SlotIndex
    +int32 TokenCost = 1
    +int32 Generation
    +float NextEligibleTime
    +float BestDistToSlot
    +float NoProgressTime
    +FCC_EngagementAssignment LastNotified
    +bool bHasNotified
    +ResetProgress() : void
  }
}

package "AI/Engagement/Slots" {
  class FCC_SlotRing <<struct>> {
    +float Radius
    +float Spacing
    +TArray<FCC_Slot> Slots
    +{static} ComputeSlotCount(InRadius, InSpacing) : int32
    +Rebuild(Center, InRadius, InSpacing, Jitter, Now) : bool
    +ClearOccupancy() : void
    +IsFree(Index) : bool
    +Occupy(Index) : void
    +Vacate(Index) : void
    +AssignIncremental(Center, Positions, InOutSlotIndices) : void
    +FindFreeSlotNear(Position) : int32
    +CountFreeUsable() : int32
  }
  class FCC_Slot <<struct>> {
    +FVector LocalOffset
    +FVector WorldLocation
    +bool bUsable
    +float CooldownUntil
    +bool bOccupied
  }
}

package "AI/Engagement/Tokens" {
  class FCC_AttackTokenPool <<struct>> {
    +int32 Capacity
    -TMap<FObjectKey, int32> Holders
    +TryAcquire(Key, Cost = 1) : bool
    +Release(Key) : bool
    +Holds(Key) : bool
    +Available() : int32
    +Used() : int32
    +PruneIf(ShouldRemove) : void
    +Reset() : void
  }
}

package "AI/Crowd" {
  class UCC_CrowdAgentStatics <<UBlueprintFunctionLibrary>> {
    +{static} ConfigureCrowdAgent(Controller) : void
    +{static} ApplyTier(Controller, Tier) : void
    +{static} SetAttacking(Controller, bAttacking) : void
  }
  class UCC_PlayerCrowdAgentComponent <<UActorComponent, ICrowdAgentInterface>> {
    -bool bRegistered
    #BeginPlay() : void
    #EndPlay(EndPlayReason) : void
    +GetCrowdAgentLocation() : FVector
    +GetCrowdAgentVelocity() : FVector
    +GetCrowdAgentCollisions(CylinderRadius, CylinderHalfHeight) : void
    +GetCrowdAgentMaxSpeed() : float
  }
}

package "AI / AbilitySystem/Abilities / Actors/Projectiles" {
  class ACC_AIController {
    -FDelegateHandle AssignmentChangedHandle
    -FDelegateHandle AttackTagHandle
    -int32 HeldGeneration
    -bool bAttackTagActive
    +ACC_AIController(ObjectInitializer)
    +OnPossess(InPawn) : void
    +OnUnPossess() : void
    +ReevaluateTarget() : void
    -ApplyTarget(NewTarget, LostLocation) : void
    -UpdateEngagement(NewTarget) : void
    -OnEngagementAssignmentChanged(Attacker, Assignment) : void
    -OnAttackTagChanged(Tag, NewCount) : void
    -BindEngagementEvents() : void
    -UnbindEngagementEvents() : void
  }
  class UCC_FollowTarget {
    -TObjectPtr<UAITask_MoveTo> MoveTask
    #float EngagedAcceptanceRadius = 30
    #float DefaultAcceptanceRadius = 50
    #ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData)
    #EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled)
    -TryGetEngagementDestination(OutDestination, OutAcceptanceRadius) : bool
    -StartMoveToTarget(Enemy, knownLocation) : void
    -StartMoveTask(AIController, Destination, AcceptanceRadius) : void
    -HandleMoveFinished(Result, AIController) : void
  }
  class UCC_Primary {
    ActivationOwnedTags += Attack
  }
  class UCC_BowShoot {
    ActivationOwnedTags += Attack
  }
  class ACC_ArrowProjectile {
    对 Pawn 改为重叠 + IsHostile
  }
}

UCC_EngagementSubsystem *-- FCC_EngagementGroup
FCC_EngagementGroup *-- FCC_SlotRing
FCC_EngagementGroup *-- FCC_AttackTokenPool
FCC_EngagementGroup *-- FCC_AttackerEntry
FCC_SlotRing *-- FCC_Slot
UCC_EngagementSubsystem ..> FCC_EngagementAssignment
UCC_EngagementSubsystem ..> FCC_EngagementSettings

ACC_AIController ..> UCC_EngagementSubsystem : 注册 / 注销 / 归还
ACC_AIController ..> UCC_CrowdAgentStatics
UCC_FollowTarget ..> UCC_EngagementSubsystem : GetAssignment
UCC_CrowdAgentStatics ..> ECC_EngagementTier
UCC_FollowTarget --> UAITask_MoveTo : MoveTask
@enduml
```

**订阅名称不要混淆**：`FCC_OnEngagementAssignmentChanged` 是委托类型，`OnAssignmentChanged` 是子系统的委托成员，`OnEngagementAssignmentChanged` 是控制器回调，`AssignmentChangedHandle` 是 `AddUObject` 返回的订阅句柄。`BindEngagementEvents` 绑定，`UnbindEngagementEvents` 使用句柄解绑。

**复用的已有能力**：`UCC_AttackIntend` 只派发攻击，不归还令牌；`UCC_KeepDistance` 只负责过近后撤；`UCC_FindRangedTarget`、`UCC_FindPlayerTarget` 不负责槽位和令牌分配。这表示围攻职责边界，不表示这些文件从未发生其他修改。

## 6. 档位状态机

这里描述的是子系统中的档位；Approaching / Attacking 是行为阶段示意，不是额外的 C++ 枚举值。档位变化通知后，行为树仍需配置对应分支才能执行移动 / 攻击。

```plantuml
@startuml 档位状态机
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
[*] --> None
None --> Queue : RegisterAttacker(Attacker, Target, bRanged)
Queue --> Surround : AssignSlots(Group, Now)\n外圈 / 远程圈分到槽位
Surround --> Queue : AssignSlots(Group, Now)\n没有可用槽位
Surround --> Engaged : TryGrantToken(Group, Now)\n同时占令牌和内圈槽位，分配新编号
Engaged --> Surround : ReleaseToken / RevokeToken\nPlaceInSurroundRing 找到外圈空位
Engaged --> Queue : ReleaseToken / RevokeToken\nPlaceInSurroundRing 没有外圈空位
state Engaged {
  [*] --> Approaching
  Approaching --> Attacking : 行为树满足攻击条件后启动能力\nAttack 标签从 0 到非零
  Approaching : CheckProgress 检查无进展时间
  Attacking : SetAttacking(this, true)
}
Engaged --> Engaged : AssignSlots 在内圈换位成功\n索引变化时 ResetProgress
Queue --> None : UnregisterAttacker / PruneInvalid\n移除条目
Surround --> None : UnregisterAttacker / PruneInvalid\n移除条目
Engaged --> None : UnregisterAttacker / PruneInvalid\n释放占用并移除条目
None --> [*] : 已退出围攻系统
note right of Surround
  bRanged 的成员只使用 Queue / Surround；
  是否射击由行为树决定。
end note
note bottom of None
  Actor 仍有效时可广播默认分配：
  Tier=None，Generation=0。
  Actor 已失效则清理占用，但跳过针对它的通知。
end note
@enduml
```

## 7. 时序图

图中 Attacker / BaseCharacter 指被控制的角色，不是 Controller；下面不使用含糊的 Self 代替两者。省略引擎生命周期回调的固定参数时，以对应头文件签名为准。

### 7.1 订阅通知 → 选中目标 → 注册到围攻组

OnPossess 先调用 BindEngagementEvents。ApplyTarget 写入目标后调用 UpdateEngagement；换目标时只调用新目标的注册，由子系统内部处理旧组，不显式广播一次 None。

```plantuml
@startuml 注册时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "ACC_AIController" as C
participant "UAbilitySystemComponent" as ASC
participant "UCC_EngagementSubsystem" as S
participant "FCC_EngagementGroup\n旧组 / 新组" as G

C -> C : OnPossess(InPawn) → BindEngagementEvents()
C -> S : OnAssignmentChanged.AddUObject(\nthis, &ThisClass::OnEngagementAssignmentChanged)
S --> C : AssignmentChangedHandle
C -> ASC : RegisterGameplayTagEvent(Attack, NewOrRemoved)\n.AddUObject(this, &ThisClass::OnAttackTagChanged)
ASC --> C : AttackTagHandle

C -> C : ApplyTarget(NewTarget, LostLocation)\n同步 CurrentTarget / 黑板 / ClosestActor
C -> C : UpdateEngagement(NewTarget)
alt NewTarget 有效
  C -> S : RegisterAttacker(BaseCharacter, NewTarget, bRanged)
  alt 已在同一目标的组内
    S -> G : 更新 Entry.bRanged
    note right of G : 不重置分配，也不重复通知
  else 新注册或换目标
    opt 原来属于其他目标
      S -> S : RemoveAttacker(AttackerKey, false)
      S -> G : Vacate 旧槽位、Tokens.Release(Key)\n移除旧 Entry，空组则移除
    end
    S -> G : 获取 / 创建新组，加入 Queue Entry
    S -> S : AllocateGeneration()
    S -> G : bForceUpdate = true
    S -> S : NotifyIfChanged(Group, Entry, true)
    S -> S : FlushNotifies()
    S -> S : OnAssignmentChanged.Broadcast(Attacker, Assignment)
    S -> C : OnEngagementAssignmentChanged(Attacker, Assignment)
    C -> C : 仅处理自己的 Pawn\n写 EngagementTier / HasAttackToken / MoveLocation
  end
else NewTarget 无效
  C -> S : UnregisterAttacker(BaseCharacter)
  S -> S : RemoveAttacker(AttackerKey, true)
  S -> S : FlushNotifies()
  opt 原条目存在且 Actor 仍有效
    S -> C : OnEngagementAssignmentChanged(Attacker, 默认分配)
    C -> C : 写 None / false，清空 MoveLocation
  end
end
@enduml
```

ReevaluateTarget() 还会在目标未变时调用 UpdateEngagement(Target) 自愈注册，默认计时器间隔为 TargetReevaluateInterval = 0.5 秒。RegisterAttacker 同目标路径是幂等操作。订阅本身不会立即补发当前标签状态。

### 7.2 子系统周期更新

Tick 每帧遍历全部现有组，满足 bForceUpdate 或组累计时间达到更新间隔时才更新；当前没有每帧最多处理几组 / 几个槽位的预算上限。新组累计时间随机错开，但强制更新仍可在同一帧发生。

```plantuml
@startuml 周期更新时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "UCC_EngagementSubsystem" as S
participant "FCC_EngagementGroup" as G
participant "FCC_SlotRing" as R
participant "FCC_AttackTokenPool" as T
participant "UNavigationSystemV1" as N
participant "ACC_AIController" as C

S -> S : Tick(DeltaTime)
loop 遍历全部 Groups，累计 Group.TimeSinceUpdate
  opt bForceUpdate 或累计时间 >= max(0.05, Settings.UpdateInterval)
    S -> S : UpdateGroup(Group, Group.TimeSinceUpdate)
    S -> S : PruneInvalid(Group)
    S -> T : Release(失效成员的 Key)\nPruneIf(条目已不在组内)
    opt 仍有成员且目标有效
      S -> S : RebuildRings(Group, Now)
      S -> N : ProjectPointToNavigation(目标中心)
      S -> R : Rebuild(Center, Radius, Spacing, Jitter, Now)
      S -> S : ValidateRing(Ring, NavCenter, bHasNavCenter)
      S -> N : ProjectPointToNavigation(槽位)
      opt 目标中心投影成功
        S -> N : NavigationRaycast(World, NavCenter, Projected.Location, HitLocation)
      end
      S -> S : CheckProgress(Group, GroupDeltaTime, Now)
      opt 无进展时间 >= Settings.StuckTimeout
        S -> S : RevokeToken(Group, Entry, bCooldownSlot, Now)
        S -> T : Release(Entry.Key)
        S -> S : AllocateGeneration()\nPlaceInSurroundRing(Group, Entry)
      end
      S -> S : AssignSlots(Group, Now)
      S -> R : InnerRing.AssignIncremental(Center, Positions, SlotIndices)
      opt 内圈重新分配后仍为 INDEX_NONE
        S -> S : RevokeToken(Group, Entry, false, Now)
      end
      S -> R : OuterRing.AssignIncremental(Center, Positions, SlotIndices)
      S -> R : RangedRing.AssignIncremental(Center, Positions, SlotIndices)
      note right of R : 非持令牌者分到槽位为 Surround，\n无槽位为 Queue
      S -> S : TryGrantToken(Group, Now)
      S -> R : InnerRing.FindFreeSlotNear(Location)
      opt 间隔足够、令牌余量足够且有候选人与内圈空位
        S -> T : TryAcquire(Best.Key, Best.TokenCost)
        S -> R : OuterRing.Vacate(Best.SlotIndex)\nInnerRing.Occupy(BestSlot)
        S -> S : AllocateGeneration()\nBest.Tier = Engaged，ResetProgress()\nGroup.LastGrantTime = Now
      end
      loop 组内各 Entry
        S -> S : NotifyIfChanged(Group, Entry, false)\n调用 BuildAssignment，变化时加入 PendingNotifies
      end
      opt cc.Engagement.Debug > 0
        S -> S : DrawDebug(Group)
      end
    end
    S -> G : TimeSinceUpdate = 0，bForceUpdate = false
  end
end
S -> S : 移除空组，再 FlushNotifies()
loop PendingNotifies 中 Actor 仍有效的通知
  S -> S : OnAssignmentChanged.Broadcast(Attacker, Assignment)
  S -> C : OnEngagementAssignmentChanged(Attacker, Assignment)
end
@enduml
```

通知条件包括强制通知、首次通知、档位 / 编号 / 令牌状态 / 位置有效性改变，或位置相对**上次通知位置**移动超过 NotifyDistance。通知先排队，周期更新的广播发生在组遍历完成后。

### 7.3 发放令牌 → 移动 → 实际攻击 → 按标签归还

下面以 UCC_AttackIntend 派发近战为例。现有 BT 攻击任务也可以直接调用 UActionComponent::TryAttack；TryAttack 属于动作组件，不是 UCC_Primary 的方法。

```plantuml
@startuml 令牌时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "UCC_EngagementSubsystem" as S
participant "ACC_AIController" as C
participant "行为树 / BT Task" as BT
participant "UCC_FollowTarget" as FT
participant "UAITask_MoveTo" as MT
participant "UCC_AttackIntend" as ATK
participant "UActionComponent" as AC
participant "UAbilitySystemComponent" as ASC
participant "UCC_Primary" as P
participant "UCC_CrowdAgentStatics" as CR

S -> S : TryGrantToken(Group, Now)\n占令牌和内圈槽位，Generation = N
S -> S : NotifyIfChanged(...) → FlushNotifies()
S -> C : OnEngagementAssignmentChanged(Attacker, Assignment)
C -> C : 写 EngagementTier / HasAttackToken / MoveLocation
C -> CR : ApplyTier(this, Engaged)
BT -> FT : FollowTarget GameplayEvent → ActivateAbility(...)
FT -> FT : TryGetEngagementDestination(Destination, AcceptanceRadius)
FT -> S : GetAssignment(BaseCharacter)
S --> FT : FCC_EngagementAssignment
FT -> FT : StartMoveTask(AIController, Destination, AcceptanceRadius)
FT -> MT : UAITask_MoveTo::AIMoveTo(...)
FT -> MT : OnMoveTaskFinished.AddUObject(...)\nReadyForActivation()
note over BT, MT
  MoveLocation 变化时由 BT 配置触发重启；
  BT 中止移动时须结束对应能力。
  EndAbility 先解绑旧任务回调，再 EndTask。
end note
MT -> FT : HandleMoveFinished(Result, AIController)
FT -> FT : EndAbility(..., bWasCancelled = !bSucceeded)
FT --> BT : BT Task 区分成功 / 失败后结束
opt 移动成功且目标有效、近战资格与实际攻击距离满足
  BT -> ATK : 激活攻击意图（可选入口）
  ATK -> AC : TryAttack(AttackAction)
  AC -> ASC : HandleGameplayEvent(ExecutionTag, &EventData)\nOptionalObject = NextAction
  ASC -> C : OnAttackTagChanged(Attack, NewCount > 0)
  C -> S : GetAssignment(BaseCharacter)
  C -> C : HeldGeneration = N
  C -> CR : SetAttacking(this, true)
  note right of CR : 路径跟随非 Idle 时先 StopMovement，\n再切 ObstacleOnly
  ASC -> P : ActivateAbility(...)\n执行实际攻击
  P -> P : 播放蒙太奇；完成 / 打断 / 取消后 EndAbility
  P -> ASC : 能力结束，GAS 移除 ActivationOwnedTags
  opt Attack 标签总计数归零
    ASC -> C : OnAttackTagChanged(Attack, 0)
    C -> CR : SetAttacking(this, false)
    C -> S : ReleaseToken(BaseCharacter, HeldGeneration)
    alt Entry 为 Engaged、Tokens.Holds(Key) 且编号匹配
      S -> S : 释放内圈与令牌，AllocateGeneration()\nNextEligibleTime = Now + GetAttackDelay(Attacker)
      S -> S : PlaceInSurroundRing(Group, Entry)\n有空位 Surround，否则 Queue
      S -> S : NotifyIfChanged(Group, Entry, true)\nFlushNotifies()
      S -> C : OnEngagementAssignmentChanged(Attacker, Assignment)
    else 编号不匹配 / 未持有 / 已不在组内
      S -> S : 忽略归还
    end
  end
end
@enduml
```

标签在 GAS 激活流程中授予，通知可能早于实际攻击能力的 ActivateAbility 正文。控制器监听的是 ASC 标签边界变化，不是蒙太奇完成委托；标签归零既可能来自正常结束，也可能来自取消、打断或激活失败后的结束。移动成功 / 失败如何映射到 BT Task 结果属于行为树接入要求，不能仅凭“能力结束”认定到达。

### 7.4 编号如何防止误释放

编号由 AllocateGeneration() 在整个子系统内分配。移除旧 Entry 不会先给这个 Entry 加一；新注册和新令牌各自获取新编号。当前 TryGrantToken 会跳过仍带 Attack 标签的成员，所以正常流程不会在旧攻击仍执行时给它再发一张令牌。

```plantuml
@startuml 编号校验时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "UAbilitySystemComponent" as ASC
participant "ACC_AIController" as C
participant "UCC_EngagementSubsystem" as S

note over ASC, S : A 持有玩家组令牌，Generation = N
ASC -> C : OnAttackTagChanged(Attack, NewCount > 0)
C -> C : HeldGeneration = N
C -> C : OnCharmedTagChanged(Tag, NewCount)\nReevaluateTarget() → ApplyTarget(B, nullptr)
C -> S : RegisterAttacker(A, B, bRanged)
S -> S : RemoveAttacker(Key, false)\n释放旧令牌、槽位，移除旧 Entry
S -> S : 新组注册，AllocateGeneration() = M\n新 Entry 初始为 Queue
note right of S
  M 不要求等于 N+1。
  旧攻击仍存在时 IsAttacking(A) 为 true，
  TryGrantToken 不会给 A 发新令牌。
end note
ASC -> C : 旧攻击结束，OnAttackTagChanged(Attack, 0)
C -> S : ReleaseToken(A, N)
S -> S : 新 Entry 无令牌 / 编号不匹配，忽略
S -> S : 后续 UpdateGroup → AssignSlots → TryGrantToken
opt A 已进入 Surround 且满足发放条件
  S -> S : 发放新令牌，AllocateGeneration() = K
  ASC -> C : 下一次攻击开始，OnAttackTagChanged(Attack, NewCount > 0)
  C -> C : HeldGeneration = K
end
@enduml
```

如果外部或延迟调用仍尝试 ReleaseToken(A, N)，即使此时 A 已持有编号 K 的令牌，也会被编号校验拒绝。

### 7.5 死亡、目标变化与异常回收

怪物死亡与控制器解除附身是两种事件，不保证死亡当帧就进入 OnUnPossess。HandleDeath 标记不存活后，组更新会兜底清理；Actor 已失效时仍释放占用，但不向该 Actor 广播退出通知。

```plantuml
@startuml 回收时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "ACC_BaseCharacter / GAS" as A
participant "ACC_AIController" as C
participant "UCC_EngagementSubsystem" as S

== 攻击者死亡，由周期更新兜底 ==
A -> A : HandleDeath()，标记不存活
S -> S : Tick → UpdateGroup → PruneInvalid(Group)
S -> S : Tokens.Release(Key)\n移除 AttackerToTarget 和组内 Entry
opt Attacker Actor 仍有效
  S -> S : PendingNotifies 加入默认分配\nTick 末尾 FlushNotifies()
  S -> C : OnEngagementAssignmentChanged(Attacker, 默认分配)
end

== 控制器解除附身 ==
C -> C : OnUnPossess() → UpdateEngagement(nullptr)
C -> S : UnregisterAttacker(BaseCharacter)
S -> S : RemoveAttacker(Key, true)\nFlushNotifies()
opt 条目存在且 Actor 仍有效
  S -> C : OnEngagementAssignmentChanged(Attacker, 默认分配)
end
C -> C : UnbindEngagementEvents()\nBaseCharacter = nullptr

== 被魅惑后换目标 ==
A -> C : OnCharmedTagChanged(Tag, NewCount)
C -> C : ReevaluateTarget() → ApplyTarget(NewTarget, nullptr)
C -> S : RegisterAttacker(BaseCharacter, NewTarget, bRanged)
S -> S : RemoveAttacker(Key, false)，再加入新组

== 目标死亡 ==
alt 控制器先发现
  C -> C : ReevaluateTarget() → ApplyTarget(nullptr, nullptr)
  C -> S : UnregisterAttacker(BaseCharacter)
else 子系统先发现
  S -> S : PruneInvalid(Group)\n目标死亡 / 失效则移除组内全部成员
end

== 卡住，或到达后未出手 ==
S -> S : CheckProgress(Group, GroupDeltaTime, Now)
S -> S : RevokeToken(Group, Entry, bCooldownSlot, Now)\n分配新编号、设置再申请冷却、尝试返回外圈

== 攻击能力被打断且 Attack 标签归零 ==
A -> C : OnAttackTagChanged(Attack, 0)
C -> S : ReleaseToken(BaseCharacter, HeldGeneration)
@enduml
```

退出时的默认分配为 None / false / bLocationValid=false / Generation=0。后续补位只通知分配发生变化的存活成员，不是无条件向所有怪物发送一次“死亡事件”。OnAssignmentChanged 不能代替角色死亡通知。

### 7.6 Crowd 接入与玩家注册

ConfigureCrowdAgent、ApplyTier、SetAttacking 都属于 UCC_CrowdAgentStatics。玩家向 UCrowdManager 注册的对象是组件 this，不是玩家 Actor 或 AI 的 PathFollowingComponent。

```plantuml
@startuml Crowd接入时序
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber
participant "ACC_AIController" as C
participant "UCC_CrowdAgentStatics" as CS
participant "UCrowdFollowingComponent" as CF
participant "UCrowdManager" as CM
participant "UCC_PlayerCrowdAgentComponent" as P
participant "UAITask_MoveTo" as MT

== 初始化 ==
C -> CF : ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>
C -> C : OnPossess(InPawn)
C -> CS : ConfigureCrowdAgent(this)
CS -> CF : SetCrowdSeparation(true)\nSetCrowdSeparationWeight(2)\nSetCrowdAnticipateTurns(true)\nSetCrowdCollisionQueryRange(600)\nSetCrowdAvoidanceQuality(Low)
P -> P : BeginPlay()，检查 Owner->HasAuthority()
opt 有权限且 UCrowdManager::GetCurrent(GetWorld()) 有效
  P -> CM : RegisterAgent(this)
  P -> P : bRegistered = true
end
note right of P : GetCrowdAgentLocation 返回脚底位置；\n玩家本身不由 Crowd 驱动

== 档位变化 ==
C -> CS : ApplyTier(this, Tier)
CS -> CF : SetCrowdAvoidanceQuality(...)\nEngaged=High，Surround=Medium，None/Queue=Low

== 攻击状态变化 ==
C -> CS : SetAttacking(this, true)
opt 路径跟随状态不是 Idle
  CS -> C : StopMovement()
end
CS -> CF : SetCrowdSimulationState(ObstacleOnly)
C -> CS : SetAttacking(this, false)
CS -> CF : SetCrowdSimulationState(Enabled)

== 移动 ==
MT -> C : 提交移动请求，进入路径跟随
CF -> CM : Crowd 路径跟随与局部避障（引擎内部）
CM --> CF : 更新移动方向 / 速度
note over CF, CM : 此处概括引擎职责，不表示项目新增了这些接口

== 结束 ==
P -> P : EndPlay(EndPlayReason)
opt bRegistered 且 CrowdManager 仍有效
  P -> CM : UnregisterAgent(this)
end
P -> P : bRegistered = false
@enduml
```

## 8. 增量槽位分配算法

```plantuml
@startuml 槽位分配活动图
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"

start
:输入：目标位置、本环的攻击者（带上次的槽位索引）\n本次重建并校验过的槽位列表;
:所有槽位清空占用标记;
partition "第一步：保留" {
  while (还有未检查的攻击者?) is (是)
    :检查该攻击者上次的槽位索引;
    if (IsFree(SlotIndex)?) then (是)
      if (方位向量为零或夹角 ≤ 90°?) then (是)
        :Occupy(SlotIndex)，保留索引;
      else (否)
        :索引置 INDEX_NONE，加入 Pending;
      endif
    else (否)
      :索引置 INDEX_NONE，加入 Pending;
    endif
  endwhile (否)
}
partition "第二步：补位" {
  :Pending 按离目标的二维距离由近到远排序;
  while (还有待分配的攻击者?) is (是)
    :取下一个待分配的攻击者;
    :SlotIndex = FindFreeSlotNear(Position);
    if (SlotIndex != INDEX_NONE?) then (是)
      :Occupy(SlotIndex);
    else (否)
      :保留 INDEX_NONE;
    endif
    :写回 InOutSlotIndices;
  endwhile (否)
}
:输出索引，由子系统 AssignSlots 处理档位\n内圈无位置则收回令牌，其他环无位置则 Queue;
stop
@enduml
```

没有全局重排，也就不需要滞回参数。玩家移动时槽位随之平移，大多数人的槽位索引不变，只是位置跟着动。

## 9. 当前接入点与资产配置要求

下表的 C++ 接入按当前工作区核对；黑板、行为树和项目参数是接入要求，不表示这些资产已全部配置或通过运行验证。

| 文件 | 改动 |
|---|---|
| `AI/CC_AIController` | `OnPossess` 配置 Crowd 并绑定事件；`ApplyTarget` / `ReevaluateTarget` 调用 `UpdateEngagement`；`OnEngagementAssignmentChanged` 写黑板并调避障质量；`OnAttackTagChanged` 记录 / 校验编号；`OnUnPossess` 先注销再解绑 |
| `UCC_FollowTarget` | `TryGetEngagementDestination` 查询分配；`StartMoveTask` 创建 `UAITask_MoveTo`；`HandleMoveFinished` 区分成功 / 失败；`EndAbility` 解绑并结束旧任务；允许重新触发 |
| `UCC_Primary` | 构造函数 `ActivationOwnedTags.AddTag(CCTags::CCAbilities::Attack)` |
| `UCC_BowShoot` | 构造函数 `ActivationOwnedTags.AddTag(CCTags::CCAbilities::Attack)` |
| `ACC_ArrowProjectile` | 对 Pawn 通道改为重叠，在重叠回调里用 `IsHostile` 判断；友方穿过，敌方命中后销毁 |
| `ACC_PlayerCharacter` | 挂 `UCC_PlayerCrowdAgentComponent` |
| `ACC_EnemyCharacter` | 删除注释掉的 RVO 代码，避免误开导致与 Crowd 冲突 |
| `UCC_Dash` | 当前保留 Attack 激活阻断，攻击中不能冲刺；若要冲刺取消攻击，需同时设计能力取消策略，不能只删除阻断 |
| 黑板资产 | 新增键 `EngagementTier`（枚举）、`MoveLocation`（向量）、`HasAttackToken`（布尔） |
| 行为树资产 | Queue / Surround → FollowTarget 去分配位置（弓兵 Surround 时检查射程与冷却后射击）；Engaged → FollowTarget，移动成功且仍有攻击资格时才攻击；`MoveLocation` 监听放在移动节点上，中止 BT 移动任务时取消对应能力，移动失败不进入攻击 |
| 项目设置 | Crowd Manager 的 `MaxAgents` 调到敌人上限 + 余量，`MaxAgentRadius` 大于最大胶囊体半径 |

**职责边界**：攻击意图和实际攻击能力不申请 / 归还令牌；`UCC_KeepDistance` 仍只做过近后撤，不能替代近战外圈移动。黑板不存编号，编号在子系统条目、分配结果和控制器的 `HeldGeneration` 中保存。

**当前源码索引**（相对本文档所在目录）：

| 文件 | 核对内容 |
|---|---|
| [CC_EngagementSubsystem.h](../../Source/GAS_Demo/Public/AI/Engagement/CC_EngagementSubsystem.h) / [.cpp](../../Source/GAS_Demo/Private/AI/Engagement/CC_EngagementSubsystem.cpp) | 接口、更新顺序、发放 / 回收、通知 |
| [CC_EngagementTypes.h](../../Source/GAS_Demo/Public/AI/Engagement/CC_EngagementTypes.h) | 枚举、配置与分配结果 |
| [CC_EngagementGroup.h](../../Source/GAS_Demo/Public/AI/Engagement/CC_EngagementGroup.h) | 组、成员、进展与通知缓存 |
| [CC_SlotRing.h](../../Source/GAS_Demo/Public/AI/Engagement/Slots/CC_SlotRing.h) / [.cpp](../../Source/GAS_Demo/Private/AI/Engagement/Slots/CC_SlotRing.cpp) | 槽位生成、占用与增量分配 |
| [CC_AttackTokenPool.h](../../Source/GAS_Demo/Public/AI/Engagement/Tokens/CC_AttackTokenPool.h) | 纯逻辑令牌池 |
| [CC_AIController.h](../../Source/GAS_Demo/Public/AI/CC_AIController.h) / [.cpp](../../Source/GAS_Demo/Private/AI/CC_AIController.cpp) | 目标同步、事件绑定、黑板回调 |
| [CC_FollowTarget.cpp](../../Source/GAS_Demo/Private/AbilitySystem/Abilities/Logic/CC_FollowTarget.cpp) | 移动能力与任务生命周期 |
| [CC_AttackIntend.cpp](../../Source/GAS_Demo/Private/AbilitySystem/Abilities/Intent/CC_AttackIntend.cpp) / [ActionComponent.cpp](../../Source/GAS_Demo/Private/Components/ActionComponent.cpp) | 攻击意图到实际能力的派发 |
| [CC_CrowdAgentStatics.cpp](../../Source/GAS_Demo/Private/AI/Crowd/CC_CrowdAgentStatics.cpp) | Crowd 参数、档位与攻击状态切换 |
| [CC_PlayerCrowdAgentComponent.cpp](../../Source/GAS_Demo/Private/AI/Crowd/CC_PlayerCrowdAgentComponent.cpp) | 玩家组件注册 / 注销 |

## 10. 测试计划

当前 [CC_EngagementTests.cpp](../../Source/GAS_Demo/Private/Tests/CC_EngagementTests.cpp) 已定义两个自动化测试：`GAS_Demo.AI.Engagement.SlotRing` 和 `GAS_Demo.AI.Engagement.TokenPool`。本次仅核对文档，没有运行测试或编译项目。

已存在的纯逻辑测试：

- `FCC_SlotRing`：槽位数随半径和间距正确计算；增量分配保留有效旧槽位；夹角超过 90° 时重新分配；槽位不够时离得远的人降档；冷却中的槽位不被分配
- `FCC_AttackTokenPool`：容量与消耗值；重复申请幂等；释放后可再申请；`PruneIf` 清理失效持有者

尚需补充的子系统集成验证（不是已有自动化测试清单）：

- 内圈没有空闲可用槽位时不发放令牌（#1、#5）
- 旧编号的 `ReleaseToken` 不影响新令牌；未持有令牌时 `ReleaseToken` 无效果（#2、#9）
- 未到达且自身静止超时：收回 + 槽位冷却；目标快速移动时不计时；到达但未出手超时：收回、不冷却（#3、#13、#19）
- 同一组两次发放间隔不小于 `MinGrantInterval`，每次更新最多发一个（#6、#12）
- 归还后 `NextEligibleTime` 生效，期间不会再被选中
- 注册 / 注销；被魅惑后转组；目标死亡时整组清空

编辑器内手动验证：

- 刷 100 近战 + 20 弓兵，Unreal Insights 记录 Crowd、子系统、感知、动画各自的耗时
- 玩家贴墙、站在走廊、站在薄墙边：确认进场人数自动减少、没有怪被分到墙后
- 玩家绕圈跑：确认交战中的怪不会因为追不上被频繁收回令牌，也不会守着对面的槽位
- 弓兵在包围圈外射击：确认箭穿过友军命中玩家
- 使用已有 `cc.Engagement.Debug 1` 调试绘制，观察每组槽位和令牌持有者

## 11. 待确认的参数

| 参数 | 建议起始值 | 根据什么调整 |
|---|---|---|
| 令牌容量 | 4 | 战斗节奏，玩家能同时应付几个人 |
| `MinGrantInterval` | 0.4 秒 | 令牌发放的间隔；默认周期更新下通常约 0.5 秒，受强制更新与帧时序影响，不能保证实际攻击起手间隔 |
| `StuckTimeout` | 2 秒 | 地形复杂程度 |
| 槽位冷却 | 3 秒 | 同上 |
| `TargetFastSpeed` | 300 | 玩家跑步速度，略低于它 |
| `StuckSelfSpeed` | 50 | 判断「真的卡住」的速度阈值 |
| 内圈半径 | 近战攻击距离 + 胶囊体半径，约 180 | 敌人攻击范围 |
| 外圈半径 | 450 | 要有压迫感但不挡住内圈进出 |
| 槽位间距 | 2 × 最大胶囊体半径 + 40 | 敌人体型 |
| `QueueMargin` | 100 | 排队区与外圈的距离 |
| 更新间隔 | 0.25 秒 | 敌人数量和玩家移动速度 |

## 12. 后续可选改进（本版不做）

| 改进 | 解决什么 | 什么时候考虑 |
|---|---|---|
| 攻击后的「恢复」阶段 | 怪物轮流上场、内外圈来回移动太多 | 实测觉得节奏机械时 |
| 能力主动上报执行结果 | 比周期检查更快发现受阻 | 2 秒的卡住判定显得太慢时 |
| 跨组槽位冲突检查 | 玩家和随从站得近时两组外圈重叠 | 双目标混战经常拥堵时 |
| 弓兵齐射预算 | 远程同时齐射 | 弓兵数量多、玩家觉得不公平时 |
| 档位 / 兵种 / 更新策略完全拆成三个维度 | 近战远程两用的怪 | 出现这类敌人时 |

## 13. 实现时的细节调整

| 位置 | 文档原写法 | 实际实现 | 原因 |
|---|---|---|---|
| 子系统创建 | `ShouldCreateSubsystem` 判断 `NetMode != NM_Client` | 只判断 World 类型（Game / PIE） | 子系统创建时客户端的网络驱动可能还没就绪，NetMode 不可靠；客户端没有 AIController，不会有注册，Tick 立即返回 |
| 持令牌者的内圈槽位失效 | 直接收回令牌 | 先在内圈里换一个空闲可用槽位，没有才收回（不冷却） | 只是这个槽位不能用，不代表内圈没位置，换位比收回更平滑 |
| `CC_FollowTarget` | 只改 `bRetriggerInstancedAbility` | 同时新增 `EndAbility`：先解绑再结束旧的 `UAITask_MoveTo` | 否则旧任务被新的移动请求顶掉时，会回调到新激活的实例，把它也结束掉 |
| 攻击开始时切 `ObstacleOnly` | 直接切换 | 路径跟随不空闲时先 `StopMovement` 再切换 | 引擎只允许在路径跟随空闲时切换 Crowd 模拟状态 |
| 远程圈半径 | 「MinRange～MaxRange 之间」 | 第一个加入该组的弓兵的 (MinRange + MaxRange) / 2，且不小于外圈半径 + 槽位间距；组存在期间不变 | 半径不变才能保证槽位索引稳定 |
| 参数配置 | 未说明 | `UCC_EngagementSubsystem` 的 `Config` 属性，在 `DefaultGame.ini` 中覆盖 | 不新增设置类 |
| 调试 | 未说明 | 控制台变量 `cc.Engagement.Debug 1` 绘制槽位和令牌持有者 | 便于调参 |
| 注册自愈 | 只在 `ApplyTarget` 中注册 | 控制器按 `TargetReevaluateInterval`（默认 0.5 秒）调用 `ReevaluateTarget`，仍有目标时再次注册；同一目标时只同步兵种 | 子系统兜底清理后，若控制器的目标没变，`ApplyTarget` 不会再被调用 |
| 槽位校验函数 | `ValidateSlots(Group)` | `RebuildRings(Group, Now)` 内按环调用 `ValidateRing(Ring, NavCenter, bHasNavCenter)` | 实际代码没有 `ValidateSlots` 函数 |
| 分配通知 | 把 `OnAssignmentChanged()` 当作控制器函数 | 子系统 `NotifyIfChanged` 排队，`FlushNotifies` 调用 `OnAssignmentChanged.Broadcast`，控制器执行 `OnEngagementAssignmentChanged` | 委托、回调、订阅句柄是不同概念 |
| 分配编号 | `uint32`，每次变化写“旧值 +1” | `int32`，用 `AllocateGeneration` 分配子系统级序号；移除条目直接丢弃旧编号 | 其他成员也会分配编号，单个成员的编号不保证连续 |
| 空闲容量 | 持有数小于 `min(配置容量, 空闲槽位数)` | 对候选人检查 `Tokens.Available() >= TokenCost`，同时要求一个空闲可用内圈槽位 | 空闲槽位数不能作为总持有量上限；当前不会动态把令牌池容量改为空闲槽位数 |
| 周期更新 | “本帧只更新一批组” | 每帧遍历全部组，按各组累计时间或强制标记决定是否更新 | 当前没有固定的每帧工作预算 |
| 死亡退出通知 | 死亡必定立即触发回调 | 主动注销或 `PruneInvalid` 清理时通知；Actor 已失效则跳过广播 | 分配委托不是死亡委托，不能要求每条销毁路径都有回调 |
