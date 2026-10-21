# 生存模式（波次刷怪 + 共享经验 + 倒计时复活）设计

状态：**v2.1，C++ 已实现（尚未在 UE 中编译与实机验证）**。实现与下文图示的差异见第 0 节，以第 0 节为准。接入、调参与排错见 [SurvivalMode-Usage.zh-CN.md](SurvivalMode-Usage.zh-CN.md)。

玩法：地图按波次定时刷怪，怪物属性随波次变强；全队共享经验，升级时每个玩家各自 Buff 三选一；玩家阵亡后倒计时复活在存活队友身边；全员阵亡则游戏结束，结算击杀数。

v2 相对 v1 的变化见第 18 节。

## 0. 实现状态与偏差（以本节为准）

| 设计稿写法 | 实际实现 | 原因 |
|---|---|---|
| 生命耗尽判定 `Health <= 0` | `Health < UCC_AttributeSet::OutOfHealthThreshold`（0.1） | 与 `UCC_ListenToHealth` 默认阈值、原 `OnHealthChange` 一致；否则暴击留下 0.05 血会播放死亡动作却不上报死亡 |
| 死亡由 Health 变化回调触发 | `OnHealthChange` 保留但不再调用 `HandleDeath`；死亡清理在 `HandleDeath`（幂等），带击杀者的广播在 `HandleOutOfHealth`（每次死亡一次） | 死亡能力（`ListenToHealth → CC_Death`）在 `SetHealth` 内同步执行，早于来源解析，两件事必须分开 |
| DeathFinished | `CC_Death::FinishOwnedAction` 在角色仍死亡时调用 `HandleDeathFinished`；复活取消死亡能力时不会误报 | |
| 作弊命令 `UCC_SurvivalCheatExtension` | 控制台命令 `cc.Survival.*`（`FAutoConsoleCommandWithWorldAndArgs`），只在服务器生效 | 不依赖 CheatManager 是否创建，监听服务器与单机都能用 |
| `ULocalPlayerSaveGame` | 普通 `USaveGame` + `AsyncLoadGameFromSlot / AsyncSaveGameToSlot`，用户索引 = 本地玩家 ControllerId | 接口稳定、无需额外配置 |
| 人数缩放用 `ScalingEffect` GE | 不需要 GE：`ACC_EnemyCharacter::InitializeAttributes` 在初始化 GE 之后直接把 MaxHealth / Health 基础值乘 `HealthScale` | 少一个资产 |
| 复活无敌用 `Status.Invincible` | 用 `Status.DamageImmunity`（`CCDamageTags::Immunity`）——伤害 GE 已按它拒绝施加；`Status.Invincible` 在本项目用于抑制 Buff | 检查项 9 已确认 |
| 离屏刷怪 `bPreferOffscreen` | `PreferredDistanceToPlayers`：优先离所有玩家都超过该距离的刷怪器（俯视角近似离屏） | 服务器不知道客户端相机 |
| 落点"寻路可达" | 只做 `ProjectPointToNavigation`（`bProjectToNavigation`），不做路径查询 | 路径查询开销大，先观察是否需要 |
| 选卡保护在 BuffSelection 配置 | `UCC_BuffSelectionComponent` 新增服务器事件 `OnServerPendingChanged / OnServerChoiceAccepted`，保护 GE 由 GameMode 施加与移除 | BuffSelection 保持与玩法无关 |
| 结算页按钮 RPC | 走 `ACC_SurvivalPlayerState::ServerRequestPlayAgain / ServerRequestReturnToLobby`（PlayerState 归本连接所有） | 不需要新的 PlayerController 子类 |
| 怪物类软引用 | 同设计；预加载当前档与下一档，未加载完成时同步加载并打警告 | |
| 平衡 CSV | `Saved/Survival/Survival_<时间>.csv`，开发构建 | |

检查项结论：7（寻敌跳过阵亡者）`ACC_AIController` 已按 `IsAlive()` 过滤；8（玩家死亡能力）`CC_Death` 只销毁敌人，玩家可复用；9 见上表。包围系统（Engagement）重构后已复查：`UCC_EngagementSubsystem` 每 0.5 秒清理目标或攻击者已死亡的登记；`ACC_AIController` 定期重新选目标时用 `IsValidTarget`（要求 `IsAlive()`）过滤，阵亡玩家会被放弃。死亡不注销感知（StimuliSource），复活后会自然重新成为目标，无需额外处理。

## 1. 规则

### 1.1 已确认

| 规则 | 结论 |
|---|---|
| 经验 | **全队共享**一条经验和等级；每升一级，**每个玩家**各得一轮三选一 |
| 阵亡 | 倒计时复活，复活点在**随机一名存活队友**身边 |
| 游戏结束 | **全员同时处于阵亡状态**即结束；单机下即"死亡即结束" |
| 击杀统计 | 个人统计（PlayerState）+ 全队统计（GameState），结算都显示 |
| 刷怪节奏 | 定时推进波次；可选"清场提前进入下一波" |
| 怪物成长 | 分档初始化 GE（质变）+ GE Level = 波次（档内平滑成长） |

### 1.2 待确认（已给默认值，实现时按默认值做，可随时改配置）

| 问题 | 默认方案 | 说明 |
|---|---|---|
| 联机时三选一不暂停，选卡期间挨打 | 选卡界面打开期间施加 `GE_ChoosingProtection`（默认减伤 50%，可配成无敌或关闭） | 单机会暂停世界，不受影响 |
| 人数越多越简单 | 怪物**数量**和**血量**按存活人数缩放（曲线配置，1 人 = 1.0） | 第 5.3 节 |
| 中途加入 | 遵循房主的 `bAllowJoinInProgress`；允许时：在队友身边出生，补发 `TeamLevel - 1` 轮三选一 | 现有 `ACC_RoomGameMode::PreLogin` 已按该选项拒绝 |
| 断线重连 | v1 不保留进度，重连按中途加入处理 | Buff 在 ASC 上，`AGameMode` 的 InactivePlayerState 也不会复制 ASC 状态 |

## 2. 现有代码的关键事实（设计依据）

| 事实 | 位置 | 对设计的影响 |
|---|---|---|
| 联机战斗 GameMode 的基类是 `ACC_RoomGameMode`（`AGameModeBase`），负责准入、无缝切图、`ReturnToLobby` | `Game/CC_RoomFramework` | `ACC_SurvivalGameMode` **继承它**；返回大厅直接复用 |
| 中途加入由房主选项 `bAllowJoinInProgress` 控制 | `ACC_RoomGameMode::CanAcceptPlayer` | 生存模式只需处理"允许加入"时的追赶 |
| 房间 PlayerState 是 `ACC_RoomPlayerState`，`CopyProperties` 在无缝切图时复制业务数据 | 同上 | 生存 PlayerState 继承它；**本局统计不跨局复制** |
| 三选一已完整实现，自带排队；无存活角色时返回 `NoTarget` 且本轮保留 | `UCC_BuffSelectionComponent` | 升级只需调用 `OfferBuffs(Pool)` |
| 刷怪器使用 `SpawnActorDeferred` | `ACC_EnemySpawner::SpawnEnemies` | 可在 `FinishSpawning` 前写入本波参数 |
| 刷怪器"不保证候选点位于可达导航区域" | `Docs/EnemySpawner.zh-CN.md` | 波次刷怪需加导航投影 |
| 属性初始化固定读 `CharacterConfig->InitializeAttributesEffect`，Level 1 | `ACC_BaseCharacter::InitializeAttributes` | 增加实例级覆盖；旧 GE 是常数时 Level 不影响 |
| `HandleDeath` 由 Health 变化回调触发，**拿不到击杀者**，也**无防重入** | `ACC_BaseCharacter::OnHealthChange` | 改为 AttributeSet 的"生命耗尽"事件（第 7.1 节） |
| 玩家 ASC 在 PlayerState，换 Pawn 不丢 GE；`UCC_BuffComponent` 在 Pawn 上 | `ACC_PlayerState` / `ACC_BaseCharacter` | 复活**复用原 Pawn**（第 13 节对照 Lyra 的取舍） |
| 死亡时 `bRemoveOnDeath=false` 的 Buff 只挂起（`bSuspendedForDeath`），**没有恢复入口** | `UCC_BuffComponent::ClearBuffs` | 新增 `ResumeAfterRevive()` |
| `HandleRespawn()` 调 `ClearBuffs()`（Reset）清空**全部** Buff | `ACC_BaseCharacter::HandleRespawn` | 复活**不能**走 `PossessedBy → HandleRespawn` |
| 已有 `CCTags.Status.Invincible`、`Status.Death` | `CC_Tags.h` | 复活保护复用 Invincible；需确认伤害 GE 已按它拒绝施加 |
| 包围系统已在目标 `!IsAlive()` 时释放槽位和令牌 | `CC_EngagementSubsystem.cpp` | 阵亡玩家不会被包围；寻敌选择是否跳过阵亡者仍需确认 |
| 已启用 `ModularGameplay` 插件、Iris 复制 | `.uproject` / `Build.cs` | 可用 `UGameStateComponent`；复制写法与项目一致 |
| UI 采用 View / Controller / Model，HUD 为 `CC_CombatMainController → Model → Widget` | `Docs/UIArchitecture.zh-CN.md` | 生存 HUD 扩展该 Controller/Model，Widget 不直接读 GameState |
| 引擎版本 5.6 | `.uproject` | 可用 `ULocalPlayerSaveGame` 做本地纪录 |

## 3. 模块划分与依赖

> 本文所有 PlantUML 图的源文件在 [`Docs/Survival/`](Survival/)（01～11，图内"见 09 / 10"指该目录的文件编号），已按 v2.1 实现更新；改图时两处同步。

```plantuml
@startuml 生存模式模块依赖
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
skinparam packageStyle rectangle

package "Game/Survival 规则（仅服务器）\nACC_SurvivalGameMode : ACC_RoomGameMode\nUCC_RespawnComponent" as Mode
package "Game/Survival 复制状态\nACC_SurvivalGameState\n├ UCC_WaveDirectorComponent\n└ UCC_TeamProgressionComponent\nACC_SurvivalPlayerState" as State
package "Game/Survival 数据\nUCC_SurvivalModeConfig（只读）\nFCC_StatTagStackContainer\nUCC_SurvivalSaveGame\nCCSurvivalTags" as Data
package "Game/Survival 调试（Private）\nCC_SurvivalDebug.cpp\ncc.Survival.* 命令 / CVar" as Dbg
package "Subsystems\nUCC_GameEventSubsystem\n（死亡事件转发）" as Events
package "UI/Survival + UI/Combat\nCC_SurvivalResultController\nCombatMainModel.GetSurvival" as SUI

package "已有：Game\nACC_RoomGameMode / RoomPlayerState" as Room
package "已有：Actors\nACC_BaseCharacter / Enemy / Player\nACC_EnemySpawner" as Char
package "已有：AbilitySystem\nUCC_AttributeSet / CC_Death" as Attr
package "已有：Components\nUCC_BuffComponent\nUCC_BuffSelectionComponent" as Comp
package "已有：Utils/Team\nUCC_TeamStatics" as Team
package "已有：UI 框架\nUIManager / Controller / Model" as UIF

Mode --> Room : 继承
Mode --> State : 推进阶段 / 转发事件
Mode --> Events : 订阅死亡事件
Mode --> Team : 击杀归属 IsHostile
Mode --> Char : Revive(Location)
Mode --> Comp : 订阅三选一服务器事件
State --> Char : FindSpawnLocation / SpawnEnemyAt
State --> Comp : OfferBuffs
State --> Data
Mode --> Data
Attr --> Char : OnOutOfHealth
Char --> Events : 死亡广播一次
Char --> Comp : ResumeAfterRevive
Dbg --> Mode : 仅调试入口
SUI --> State : 只读复制数据 + 消息
SUI --> UIF
SUI --> Data : 本地最佳纪录

note bottom of State
  波次与经验组件互不依赖，
  各自持有自己的复制数据，
  界面直接绑定对应组件
end note
note bottom of Events
  只有角色发布、只有 GameMode 订阅；
  角色不依赖 Game/Survival
end note
@enduml
```

禁止的依赖：波次 ↔ 经验 ↔ 复活组件互相调用；`Character` → `Survival/*`（角色不知道自己处于生存模式）；UI → 任何服务器逻辑（只读复制数据）；任何 Survival 代码出现针对具体 Buff 的判断（硬性规则 1）；调试扩展以外的代码依赖 `Debug`。

**为什么波次与经验放在 GameState 组件上**：两者都有需要复制给所有客户端的状态（波次、倒计时、等级、经验条）。做成 `UGameStateComponent` 后，逻辑和它的复制数据放在同一个类里，服务器逻辑用 `HasAuthority()` 保护，GameState 本身不会膨胀成大杂烩。这也是 Lyra 的做法（模式功能以组件挂到 GameState 上）。复活只有服务器逻辑，个人倒计时在 PlayerState 上，所以留在 GameMode。

## 4. 总体架构（服务器 / 客户端）

```plantuml
@startuml 生存模式总体架构
skinparam componentStyle rectangle
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"

package "服务器" {
  package "ACC_SurvivalGameMode : ACC_RoomGameMode" {
    [GameMode\n阶段切换 / 事件路由\n击杀归属 / 全灭判定\n统计订阅 / 选卡保护] as GM
    [UCC_RespawnComponent\n复活倒计时 / 选复活点] as RC
  }
  [UCC_GameEventSubsystem\nOnCharacterDied\nOnCharacterDeathFinished] as GES
  [ACC_EnemySpawner × N\nbManagedByWaveDirector] as SP
  [ACC_EnemyCharacter\nSpawnParams(Transient)] as EN
  [ACC_PlayerCharacter\nRevive()] as PC
  [UCC_AttributeSet\nOnOutOfHealth（Health < 0.1）] as AS
  [CC_Death\n结束时 HandleDeathFinished] as DA
  [UCC_BuffSelectionComponent\nOnServerPendingChanged\nOnServerChoiceAccepted] as BS
  [UCC_BuffComponent\nResumeAfterRevive] as BC
}

package "复制状态" {
  package "ACC_SurvivalGameState" {
    [Phase / 开始结束时间\nTeamStats（TagStack）\nMulticastMatchMessage] as GS
    [UCC_WaveDirectorComponent\n服务器：计时 / 刷怪队列 / 回收 / CSV\n复制：CurrentWave / NextWaveServerTime / AliveEnemyCount] as WD
    [UCC_TeamProgressionComponent\n服务器：加经验 / 升级\n复制：TeamLevel / TeamExperience] as TP
  }
  [ACC_SurvivalPlayerState\nStats（TagStack）\nLifeState / RespawnServerTime\nServerRequestPlayAgain / ReturnToLobby] as PS
}

package "客户端（每个本地玩家）" {
  [CC_CombatMainController\nGetSurvival 快照] as HUD
  [结算页\nCC_SurvivalResultController] as RES
  [UCC_SurvivalSaveGame\n本地最佳纪录] as SAVE
  [三选一界面（已有）] as SEL
}

database "UCC_SurvivalModeConfig\n(DataAsset，只读)" as CFG

AS --> EN : OnOutOfHealth
AS --> PC : OnOutOfHealth
EN --> GES : 死亡广播
PC --> GES : 死亡广播
DA --> GES : DeathFinished
GES --> GM
GM --> WD : NotifyEnemyRemoved
GM --> TP : AddExperience
GM --> RC : SchedulePlayer / NotifyDeathFinished
GM --> GS
GM --> PS
BS --> GM : 选卡开始/结束、已选
WD --> SP : FindSpawnLocation\nSpawnEnemyAt（分帧）
SP --> EN : Deferred Spawn + SpawnParams
TP --> BS : OfferBuffs(Pool)
RC --> PC : Revive(Location)
PC --> BC : ResumeAfterRevive
WD ..> CFG
TP ..> CFG
RC ..> CFG

GS ..> HUD : OnRep / 消息
WD ..> HUD : OnRep
TP ..> HUD : OnRep
PS ..> HUD : OnRep（复活倒计时）
GS ..> RES : Phase = GameOver
RES --> SAVE : 异步读写
RES --> PS : Server RPC
BS ..> SEL : 复制当前一轮
@enduml
```

## 5. 类与数据

### 5.1 类图（新增与修改）

```plantuml
@startuml 生存模式类图
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
skinparam classAttributeIconSize 0
hide empty members

package "已有：Game" {
  class ACC_RoomGameMode <<existing>>
  class ACC_RoomPlayerState <<existing>>
  class ACC_GameState <<existing>>
}

package "Game/Survival：规则" {
  class ACC_SurvivalGameMode <<new>> {
    # Config : UCC_SurvivalModeConfig*
    # StartTimeout : float = 10
    # Respawn : UCC_RespawnComponent*
    --
    + StartMatch() / EndMatch()
    + IsMatchPlaying() : bool
    + HandlePlayerRevived(PS)
    + RequestPlayAgain() / RequestReturnToLobby()
    + DebugAddExperience / DebugReviveAll / DebugToggleGodMode
    # InitGame()：?SurvivalConfig= / ?SurvivalSeed=
    # HandleStartingNewPlayer()：中途加入追赶
    # Logout()
    - HandleCharacterDied(FCC_DeathInfo)
    - HandleCharacterDeathFinished(Character)
    - ShouldCreditKill(Info) : bool
    - IsTeamWiped() : bool
    - BindPlayer / UnbindPlayer：统计、选卡保护
    - SetChoosingProtection(PS, bEnable)
  }
  class UCC_RespawnComponent <<new>> {
    + SchedulePlayer(PS)
    + NotifyDeathFinished(PS)
    + Cancel(PS) / CancelAll()
    + ReviveNow(PS) : bool
    + FindReviveLocation(Anchor, Reviving) : FVector
    - TryRevive(PS, bForce)
    - PickAnchor(Exclude)
    - Pending : TMap<PS, FPending>
  }
}

package "Game/Survival：复制状态" {
  class ACC_SurvivalGameState <<new>> {
    + MatchStartServerTime / MatchEndServerTime [Rep]
    - Phase : ECC_MatchPhase [Rep]
    - TeamStats : FCC_StatTagStackContainer [Rep]
    # ResultScreenClass / ResultScreenLayer
    --
    + GetPhase() / GetTeamStat(Tag)
    + GetMatchElapsedSeconds()
    + GetSecondsUntilServerTime(Time)
    + GetSurvivalPlayers()
    + SetPhase() / AddTeamStat()（服务器）
    + MulticastMatchMessage [NetMulticast, Unreliable]
    + OnPhaseChanged / OnTeamStatsChanged / OnMatchMessage
  }
  class UCC_WaveDirectorComponent <<new, GameStateComponent>> {
    - CurrentWave / NextWaveServerTime / AliveEnemyCount [Rep]
    .. 仅服务器 ..
    - SpawnQueue : TArray<FCC_PendingSpawn>
    - AliveEnemies / FarTimes / Spawners
    - RandomStream
    --
    + StartDirector(Config, Seed) / StopDirector()
    + NotifyEnemyRemoved(Enemy)
    + DebugSetWave / DebugSkipWave / DebugClearEnemies
    - AdvanceWave() / TickSpawnQueue()
    - CleanupStrayEnemies()
    - PickSpawner() / PickEnemyClass()
    - MulticastSpawnTelegraph [NetMulticast, Unreliable]
    + OnWaveStateChanged / OnSpawnTelegraph
  }
  class UCC_TeamProgressionComponent <<new, GameStateComponent>> {
    - TeamLevel / TeamExperience / ExperienceToNextLevel [Rep]
    --
    + StartProgression(Config) / StopProgression()
    + AddExperience(Amount) : int32
    + GrantCatchUpOffers(PC)
    + OnProgressChanged
  }
  class ACC_SurvivalPlayerState <<new>> {
    - Stats : FCC_StatTagStackContainer [Rep]
    - LifeState : ECC_PlayerLifeState [Rep]
    - RespawnServerTime : double [Rep]
    --
    + GetStat / GetLifeState / GetRespawnRemainingSeconds
    + AddStat / SetLifeState / SetRespawnServerTime（服务器）
    + ServerRequestPlayAgain() [Server]
    + ServerRequestReturnToLobby() [Server]
    + CanControlMatch() : bool
    + OnSurvivalStateChanged
  }
  enum ECC_MatchPhase {
    WaitingToStart
    Playing
    GameOver
  }
  enum ECC_PlayerLifeState {
    Alive
    Dying
    Dead
  }
}

package "Subsystems" {
  class UCC_GameEventSubsystem <<new, WorldSubsystem>> {
    + OnCharacterDied
    + OnCharacterDeathFinished
  }
  class FCC_DeathInfo <<struct>> {
    + Victim / Killer / DamageCauser
    + KillerPlayerState
  }
}

package "已有：修改" {
  class UCC_AttributeSet <<modified>> {
    + OnOutOfHealth : FCC_OutOfHealthEvent
    + OutOfHealthThreshold = 0.1 {static}
    - bOutOfHealth : bool
  }
  class ACC_BaseCharacter <<modified>> {
    + HandleDeath()：幂等
    + HandleDeathFinished()
    # BindOutOfHealth(Attributes)
    # HandleOutOfHealth(Instigator, Causer, Spec, Magnitude)
    - bDeathReported : bool
  }
  class ACC_PlayerCharacter <<modified>> {
    + Revive(Location, ReviveEffect) : bool
  }
  class ACC_EnemyCharacter <<modified>> {
    - SpawnParams : FCC_EnemySpawnParams [Transient]
    + SetSpawnParams(Params)
    + GetExperienceReward() : float
    # InitializeAttributes()：InitEffect @ 波次 + HealthScale
  }
  class ACC_EnemySpawner <<modified>> {
    + bManagedByWaveDirector : bool
    + FindSpawnLocation(Class, bProjectToNav, Random, Out) : bool
    + SpawnEnemyAt(Class, Location, Params)
    + SpawnEnemies()：行为不变
  }
  class UCC_BuffComponent <<modified>> {
    + ResumeAfterRevive()
  }
  class UCC_BuffSelectionComponent <<modified>> {
    + OnServerPendingChanged
    + OnServerChoiceAccepted
  }
  class UCC_CharacterConfig <<modified>> {
    + ExperienceReward : float = 10
  }
}

ACC_RoomGameMode <|-- ACC_SurvivalGameMode
ACC_RoomPlayerState <|-- ACC_SurvivalPlayerState
ACC_GameState <|-- ACC_SurvivalGameState
ACC_SurvivalGameMode *-- UCC_RespawnComponent
ACC_SurvivalGameMode ..> UCC_GameEventSubsystem : 订阅
ACC_SurvivalGameMode ..> UCC_BuffSelectionComponent : 订阅服务器事件
ACC_SurvivalGameMode --> ACC_SurvivalGameState
ACC_SurvivalGameState *-- UCC_WaveDirectorComponent
ACC_SurvivalGameState *-- UCC_TeamProgressionComponent
ACC_SurvivalGameState --> ECC_MatchPhase
ACC_SurvivalPlayerState --> ECC_PlayerLifeState
UCC_GameEventSubsystem ..> FCC_DeathInfo
UCC_AttributeSet ..> ACC_BaseCharacter : OnOutOfHealth
ACC_BaseCharacter ..> UCC_GameEventSubsystem : 发布
ACC_BaseCharacter <|-- ACC_PlayerCharacter
ACC_BaseCharacter <|-- ACC_EnemyCharacter
ACC_PlayerCharacter ..> UCC_BuffComponent : 复活时恢复
UCC_WaveDirectorComponent --> ACC_EnemySpawner
ACC_EnemySpawner ..> ACC_EnemyCharacter : 写 SpawnParams
ACC_EnemyCharacter ..> UCC_CharacterConfig : 基础经验
UCC_TeamProgressionComponent ..> UCC_BuffSelectionComponent : OfferBuffs
UCC_RespawnComponent ..> ACC_PlayerCharacter : Revive
@enduml
```

### 5.2 配置数据

全部是只读配置（硬性规则 6）。`ACC_SurvivalGameMode` 类默认值引用 `DA_SurvivalMode`；地图可在 URL 选项 `?SurvivalConfig=` 覆盖，用于不同地图不同难度。

```plantuml
@startuml 生存模式配置数据
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
skinparam classAttributeIconSize 0
hide empty members

class UCC_SurvivalModeConfig <<PrimaryDataAsset，只读>> {
  + Waves : FCC_WaveSettings
  + Spawning : FCC_SpawnPacingSettings
  + PlayerScaling : FCC_PlayerScalingSettings
  + Progression : FCC_ProgressionSettings
  + Respawn : FCC_RespawnSettings
  --
  + FindTier(Settings, Wave) {static}
  + ComputeCount(Tier, Wave, CountScale) {static}
  + EvaluateMultiplier(Curve, X) {static}
  + ComputeExperienceToNext(Settings, Level) {static}
  + ApplyExperience(Settings, Level, Exp, Amount) {static}
  + ComputeRespawnDelay(Settings, DeathCount) {static}
  + IsDataValid()
}

class FCC_WaveSettings <<struct>> {
  + FirstWaveDelay = 5
  + WaveInterval = 20
  + MaxAliveEnemies = 60
  + bAdvanceWhenCleared = false
  + Tiers : TArray<FCC_WaveTier>
}

class FCC_WaveTier <<struct>> {
  + StartWave（首档 = 1，严格递增）
  + InitEffect : TSubclassOf<UGameplayEffect>
  + Enemies : TArray<FCC_WeightedEnemy>
  + BaseCount = 5
  + CountPerWave = 1
  + ExperienceMultiplier = 1
}

class FCC_WeightedEnemy <<struct>> {
  + EnemyClass : TSoftClassPtr<ACC_EnemyCharacter>
  + Weight = 1
}

class FCC_SpawnPacingSettings <<struct>> {
  + SpawnsPerTick = 2
  + TickInterval = 0.1
  + TelegraphDuration = 0.8
  + TelegraphCue : FGameplayTag
  + MinDistanceToPlayers = 800
  + PreferredDistanceToPlayers = 1600
  + bProjectToNavigation = true
  + LeashDistance = 4000
  + LeashTime = 8
}

class FCC_PlayerScalingSettings <<struct>> {
  + CountMultiplier : FRuntimeFloatCurve
  + HealthMultiplier : FRuntimeFloatCurve
}
note right of FCC_PlayerScalingSettings
  X = 存活玩家数，Y = 倍率
  没有关键帧时视为 1
end note

class FCC_ProgressionSettings <<struct>> {
  + ExperienceToNext : FRuntimeFloatCurve
  + BaseExperienceToNext = 20
  + ExperienceToNextPerLevel = 10
  + BuffPool : UCC_BuffOfferPool*
  + MaxLevel = 0（不限）
  + ChoosingProtectionEffect : TSubclassOf<UGameplayEffect>
}

class FCC_RespawnSettings <<struct>> {
  + BaseDelay = 5
  + DelayPerDeath = 2
  + MaxDelay = 20
  + MinRadius = 150
  + MaxRadius = 400
  + DeathFinishedTimeout = 5
  + ReviveEffect : TSubclassOf<UGameplayEffect>
}
note right of FCC_RespawnSettings
  ReviveEffect 建议：回满血
  + Status.DamageImmunity 2 秒 + 复活 Cue
end note

class FCC_EnemySpawnParams <<struct，运行时>> {
  + InitEffect
  + EffectLevel（= 波次）
  + HealthScale
  + ExperienceMultiplier
  + WaveIndex
  + bIsSet
}

class UCC_SurvivalSaveGame <<USaveGame，客户端本地>> {
  + SaveVersion
  + BestWave / BestKills / BestSurvivalTime
}

UCC_SurvivalModeConfig *-- FCC_WaveSettings
UCC_SurvivalModeConfig *-- FCC_SpawnPacingSettings
UCC_SurvivalModeConfig *-- FCC_PlayerScalingSettings
UCC_SurvivalModeConfig *-- FCC_ProgressionSettings
UCC_SurvivalModeConfig *-- FCC_RespawnSettings
FCC_WaveSettings *-- "1..*" FCC_WaveTier
FCC_WaveTier *-- "1..*" FCC_WeightedEnemy
FCC_WaveTier ..> FCC_EnemySpawnParams : 每波生成
@enduml
```

### 5.3 数值来源

- **怪物成长**：`InitEffect` 修正器使用 ScalableFloat，Curve 指向 CurveTable 行（如 `EnemyHealth_T2`），横轴为 GE Level = 波次。CurveTable 从 CSV 导入，策划可在表格软件里调整。
- **人数缩放**：数量 = `ComputeCount × CountMultiplier(存活人数)`；血量在 `InitEffect` 之后再施加 `ScalingEffect`，用 SetByCaller `Data.Survival.HealthScale` 乘 MaxHealth 并回满 Health。存活人数在**每波开始时**取样，波中有人阵亡不回调已刷出的怪。
- **经验**：怪物奖励 = `CharacterConfig.ExperienceReward × ExperienceMultiplier`。不按人数缩放（全队共享已经天然平衡）。
- **怪物类引用用软引用**：`TSoftClassPtr`，`Start()` 时异步预加载当前档和下一档，避免首次刷新时同步加载卡顿。

### 5.4 统计项（TagStack）

参考 Lyra 的 PlayerState `StatTags`：统计用"标签 → 计数"的 FastArray 容器，新增统计不改数据结构，结算页遍历显示。

| 标签 | 个人 | 全队 | 来源 |
|---|---|---|---|
| `Survival.Stat.Kills` | ✓ | ✓ | 死亡事件，击杀归属 |
| `Survival.Stat.Deaths` | ✓ | ✓ | 玩家死亡 |
| `Survival.Stat.Revives` | ✓ |  | 复活完成 |
| `Survival.Stat.DamageDealt` | ✓ | ✓ | 监听玩家 ASC 的 `Event.Combat.DamageDealt`（取整累计） |
| `Survival.Stat.DamageTaken` | ✓ |  | 监听 `Event.Combat.DamageTaken` |
| `Survival.Stat.BuffsChosen` | ✓ |  | 三选一 `Accepted` |

GameMode 在玩家加入时订阅其 ASC 的伤害事件，`Logout` 和 `EndMatch` 时退订（与订阅对称）。

## 6. 状态机

### 6.1 对局阶段（`ACC_SurvivalGameState::Phase`）

```plantuml
@startuml 对局阶段
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"

[*] --> WaitingToStart
WaitingToStart --> Playing : 所有已连接玩家的 Pawn 存活就绪\n或等待超过 StartTimeout（默认 10 秒）\n→ StartMatch()
Playing --> GameOver : 全员 LifeState != Alive\n或最后一名玩家退出（下一帧判定）
GameOver --> [*] : 房主：再来一局（ServerTravel ?Restart）\n或返回大厅（ReturnToLobby）

state Playing {
  [*] --> Warmup
  Warmup --> WaveActive : FirstWaveDelay 结束
  WaveActive --> WaveActive : WaveInterval × WaveIntervalScale 到时\n(或清场且 bAdvanceWhenCleared)\n→ AdvanceWave()
}

state GameOver {
  GameOver : 进入时（服务器，按顺序）：
  GameOver : 1. WaveDirector.StopDirector()（清空队列、写最后一行 CSV）
  GameOver : 2. Progression.StopProgression()
  GameOver : 3. Respawn.CancelAll()
  GameOver : 4. 每个玩家 BuffSelection.ClearOffers()（同时撤掉选卡保护）
  GameOver : 5. 冻结怪物：StopMovement + BrainComponent.StopLogic
  GameOver : 6. 记录 MatchEndServerTime，退订所有玩家的统计事件
  GameOver : 7. Phase = GameOver（复制 → 客户端打开结算页）
  GameOver : 之后到达的死亡 / 伤害事件一律忽略
}
@enduml
```

未采用的方案：`AGameMode` 的 `MatchState`（ShooterGame / UT 的做法）。因为 `ACC_RoomGameMode` 继承的是 `AGameModeBase`，换基类会影响大厅流程；Lyra 也是在 `GameModeBase` 上自建阶段。Lyra 用"阶段 = 一个 GameplayAbility"的阶段子系统，适合阶段多且嵌套的模式；本模式只有三个阶段，用枚举即可，以后阶段变多再迁移。

### 6.2 玩家生命状态（`ACC_SurvivalPlayerState::LifeState`）

```plantuml
@startuml 玩家生命状态
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"

[*] --> Alive
Alive --> Dying : 生命耗尽（OnCharacterDied）\nStats.Deaths++\nBuff 挂起（bRemoveOnDeath=false 的保留）\n未全灭 → 开始复活倒计时
Dying --> Dead : CC_Death 结束（OnCharacterDeathFinished）\n视角切到存活队友
Dead --> Alive : 倒计时结束\nRevive(Location)\nResumeAfterRevive\nStats.Revives++
Dying --> Alive : 倒计时先结束且超过 DeathFinishedTimeout\n（玩家没有死亡能力时的兜底）

note right of Dying
  全灭判定按 LifeState != Alive，
  Dying 也算阵亡
end note
note left of Dead
  RespawnServerTime 复制给客户端显示倒计时
  阵亡期间：
  - 共享经验照常增长，升级照常排队三选一
  - 选择返回 NoTarget，本轮保留，复活后再选
  - AI 选目标要求 IsAlive，会放弃阵亡者
end note
@enduml
```

死亡分 Started / Finished 两段，参考 Lyra 的 `ULyraHealthComponent`：击杀统计、经验、全灭判定都在 **Started** 立即执行；尸体处理、视角切换、复活传送等**依赖动作结束**的事放在 **Finished**。

## 7. 时序图

### 7.1 死亡事件与击杀归属（地基）

参考 Lyra `ULyraHealthSet`：AttributeSet 在生命归零的那一次 GE 执行里直接广播"生命耗尽"，带上 Instigator / Causer / Spec。这比"扣血前记录来源、再由 Health 变化回调取用"更直接，也不依赖回调顺序。

```plantuml
@startuml 死亡事件与击杀归属
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber

participant "UCC_DamageStatics\n::ApplyDamage" as DMG
participant "UCC_AttributeSet\n(受害者)" as AS
participant "UCC_ListenToHealth\n→ CC_Death" as LH
participant "ACC_BaseCharacter\n(受害者)" as CH
participant "UCC_GameEventSubsystem" as GES
participant "ACC_SurvivalGameMode" as GM

DMG -> AS : 伤害 GE 执行（Damage 中间属性）
activate AS
AS -> AS : SetHealth(Health - Damage)
AS -> LH : Health 变化回调（同步，早于下面的广播）
activate LH
LH -> CH : 死亡能力激活 → HandleDeath()
note right of CH
  HandleDeath 幂等：
  IsAlive() 为 false 时直接返回
  清理：SetAlive(false)、ClearBuffs(OwnerDeath)
end note
deactivate LH
AS -> AS : SendDamageEvents（DamageTaken / DamageDealt，原逻辑）
alt Health < 0.1 且 !bOutOfHealth
  AS -> AS : bOutOfHealth = true\n从 Context 解析 Instigator（来源 ASC 的 Avatar）/ Causer
  AS -> CH : OnOutOfHealth(Instigator, Causer, Spec, Magnitude)
  activate CH
  alt bDeathReported 或 属性集已不属于本 Pawn
    CH --> AS : 忽略
  else 首次
    CH -> CH : bDeathReported = true\nHandleDeath()（已清理则跳过）
    CH -> GES : OnCharacterDied(Victim, Killer, Causer, KillerPlayerState)
    GES -> GM : HandleCharacterDied(Info)
    note right of GM : 见 09（敌人）/ 10（玩家）
  end
  deactivate CH
end
deactivate AS

... 死亡蒙太奇播完 ...
LH -> CH : CC_Death.FinishOwnedAction\n角色仍死亡 → HandleDeathFinished()
CH -> GES : OnCharacterDeathFinished(Character)
GES -> GM : HandleCharacterDeathFinished

note over AS
  Health 回到 >= 0.1（复活回血、重新初始化）时
  PostAttributeChange 清除 bOutOfHealth；
  复活 / 重生时角色清除 bDeathReported
end note
@enduml
```

### 7.2 波次刷怪（分帧队列 + 出生预警）

一次在同一帧生成十几个 Character 会明显卡顿（每个都要初始化 ASC、授予能力、注册 Crowd）。成熟的生存类游戏都会把一波拆成队列分帧生成，并在出生点先播 0.5～1 秒的预警特效，既平滑开销，也让玩家有反应时间。

```plantuml
@startuml 波次刷怪
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber

participant "UCC_WaveDirector\nComponent" as WD
participant "ACC_EnemySpawner" as SP
participant "客户端\n(Cue / OnSpawnTelegraph)" as CUE
participant "ACC_EnemyCharacter" as EN
participant "ASC(怪物)" as ASC

[-> WD : WaveTimer 到时（游戏时间，单机三选一暂停时同步暂停）
activate WD
WD -> WD : 写上一波平衡 CSV\nCurrentWave++，Tier = FindTier(Wave)
WD -> WD : Players = 存活玩家数（本波取样）\nCount = ComputeCount(Tier, Wave, CountMultiplier(Players) × SpawnCountScale)\n截断到 MaxAlive - 存活 - 队列
WD -> WD : Params = { InitEffect, EffectLevel = Wave,\nHealthScale = HealthMultiplier(Players),\nExperienceMultiplier, bIsSet }
loop Count 次
  WD -> WD : PickEnemyClass：按权重（RandomStream）\n未预加载完成则同步加载并警告
  WD -> WD : PickSpawner：受控刷怪器中\n离所有玩家 ≥ Preferred → ≥ MinDistance → 最远
  WD -> SP : FindSpawnLocation(Class, bProjectToNavigation, RandomStream)
  SP --> WD : 地面 + 坡度 + NavMesh 投影 + 胶囊无阻挡
  WD -> WD : SpawnQueue.Add({Spawner, Class, Location, ReadyTime = Now + Telegraph, Params})
  WD -> CUE : MulticastSpawnTelegraph(Location, Duration, Cue)
end
WD -> WD : 广播 Survival.Msg.WaveStarted\n预加载下一档怪物类\nScheduleNextWave(Interval × WaveIntervalScale)
deactivate WD

loop QueueTimer 每 TickInterval
  WD -> WD : 取 ReadyTime 已到的前 SpawnsPerTick 个
  WD -> SP : SpawnEnemyAt(Class, Location, Params)
  activate SP
  SP -> EN : SpawnActorDeferred → SetSpawnParams → FinishSpawning
  EN -> ASC : BeginPlay → InitializeAttributes：\nInitEffect @ Level = Wave\nMaxHealth / Health 基础值 × HealthScale
  SP --> WD : Enemy（失败则丢弃）
  deactivate SP
  WD -> WD : AliveEnemies.Add，TeamStats.EnemiesSpawned++
end

loop CleanupTimer 每 2 秒
  WD -> WD : 清除已死亡 / 已销毁的记录
  WD -> EN : 离所有存活玩家 > LeashDistance\n持续 ≥ LeashTime → Destroy（不给经验、不计击杀）
end
@enduml
```

### 7.3 敌人阵亡：击杀统计与共享经验

```plantuml
@startuml 共享经验与升级
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber

participant "ACC_SurvivalGameMode" as GM
participant "UCC_TeamStatics" as TEAM
participant "UCC_WaveDirector" as WD
participant "ACC_SurvivalPlayerState\n(击杀者)" as KPS
participant "ACC_SurvivalGameState" as GS
participant "UCC_TeamProgression" as TP
participant "UCC_BuffSelection\n(每个玩家)" as BS

[-> GM : HandleCharacterDied(Info)，Victim 是敌人
activate GM
opt Phase != Playing
  GM -> GM : 忽略
end
GM -> WD : NotifyEnemyRemoved(Victim)
GM -> TEAM : ShouldCreditKill：IsHostile(Killer, Victim) 且\n（Killer 有 PlayerState，或与任一玩家不敌对）
alt 玩家方击杀（含被魅惑的友军、以后的召唤物）
  opt KillerPlayerState 有效
    GM -> KPS : Stats[Kills]++
  end
  GM -> GS : TeamStats[Kills]++
  GM -> TP : AddExperience(ExperienceReward × ExperienceMultiplier × XPScale)
  activate TP
  TP -> TP : ApplyExperience：while 经验 ≥ 升级所需（未达 MaxLevel）\n扣除并 TeamLevel++
  opt 升了 N 级
    loop 每个 PlayerController（含阵亡玩家）
      TP -> BS : OfferBuffs(BuffPool) × N（自带排队）
    end
    TP -> GS : MulticastMatchMessage(Survival.Msg.LevelUp)
  end
  deactivate TP
else 非玩家方击杀 / 无击杀者
  GM -> GM : 不计数、不给经验
end
deactivate GM

== 选卡期间保护（联机；单机打开三选一会暂停世界，不施加） ==
BS -> GM : OnServerPendingChanged(true)
GM -> GM : 施加 ChoosingProtectionEffect，记录句柄
BS -> GM : OnServerChoiceAccepted(Option)
GM -> KPS : Stats[BuffsChosen]++
BS -> GM : OnServerPendingChanged(false)（选完 / ClearOffers）
GM -> GM : 移除保护 GE（与施加对称）
@enduml
```

### 7.4 玩家阵亡、倒计时复活、全灭结束

```plantuml
@startuml 玩家阵亡与复活
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber

participant "ACC_SurvivalGameMode" as GM
participant "UCC_RespawnComponent" as RC
participant "ACC_SurvivalPlayerState\n(阵亡者)" as PS
participant "ACC_PlayerCharacter\n(阵亡者，复用同一 Pawn)" as PC
participant "UCC_BuffComponent" as BC
participant "UNavigationSystemV1" as NAV

[-> GM : HandleCharacterDied(Info)，Victim 是玩家
activate GM
GM -> PS : Stats[Deaths]++，LifeState = Dying
GM -> GM : 广播 Survival.Msg.PlayerDown
alt IsTeamWiped（没有 Alive 且仍在线的玩家）
  GM -> GM : EndMatch()（见 05）
else
  GM -> RC : SchedulePlayer(PS)
  RC -> PS : RespawnServerTime = Now + ComputeRespawnDelay(Deaths)
end
deactivate GM

[-> GM : HandleCharacterDeathFinished（CC_Death 播完）
GM -> PS : LifeState = Dead
GM -> GM : SetViewTargetWithBlend(存活队友)（服务器设置，自动同步给客户端）
GM -> RC : NotifyDeathFinished(PS)

... 倒计时结束 ...

RC -> RC : HandleCountdownFinished
alt LifeState == Dying（动作未播完）
  RC -> RC : 等 NotifyDeathFinished；\n超过 DeathFinishedTimeout 则强制复活
else
  RC -> RC : TryRevive：Phase 必须为 Playing\nAnchor = 随机一名存活玩家
  RC -> NAV : Anchor 周围 [MinRadius, MaxRadius] 8 个方向\nProjectPointToNavigation + 胶囊无阻挡（全失败用 Anchor 位置）
  RC -> PC : Revive(Location, ReviveEffect)
  activate PC
  PC -> PC : ResetForRespawn（先标记存活）\nCancelAbilities(Death)\nTeleportTo，恢复碰撞 / 移动
  PC -> PC : 施加 ReviveEffect（回满血 + Status.DamageImmunity）\n仍无血则代码兜底回满
  PC -> BC : ResumeAfterRevive()\n（撤销 ClearBuffs(OwnerDeath) 的挂起）
  deactivate PC
  RC -> GM : HandlePlayerRevived(PS)
  GM -> PS : LifeState = Alive，RespawnServerTime = 0，Stats[Revives]++
  GM -> GM : 视角切回自己，广播 Survival.Msg.PlayerRevived
end

== 断线 ==
[-> GM : Logout(PC)
GM -> RC : Cancel(PS)
GM -> GM : 解除统计 / 选卡订阅（撤保护）\n下一帧判定是否全灭

note over GM, PC
  复活不走 PossessedBy → HandleRespawn：
  那条路径会 ClearBuffs(Reset) 清掉所有升级 Buff。
end note
@enduml
```

### 7.5 结算、纪录与再来一局

```plantuml
@startuml 结算
skinparam shadowing false
skinparam defaultFontName "Microsoft YaHei"
autonumber

participant "ACC_SurvivalGameState\n(客户端副本)" as GS
participant "UCC_UIManagerSubsystem\n/ 根布局" as UIM
participant "CC_SurvivalResultController" as CTRL
participant "CC_SurvivalResultModel\n/ 结算页蓝图" as VIEW
participant "UCC_SurvivalSaveGame" as SAVE
participant "ACC_SurvivalPlayerState" as PS
participant "ACC_SurvivalGameMode\n(服务器)" as GM

GS -> GS : OnRep_Phase(GameOver)（监听服务器主机在 SetPhase 中直接执行）
GS -> UIM : 每个本地玩家 ShowScreen(ResultScreenClass, UI.Layer.Menu)\n根布局过渡中则用核心 Ticker 每 0.25 秒重试（最多 20 次）
UIM -> CTRL : 页面激活 → OnActivated
CTRL -> GS : 波次 / 等级 / 全队击杀 / 存活时长
CTRL -> PS : 本地 Stats、CanControlMatch
CTRL -> VIEW : SetResult（先显示）
CTRL -> SAVE : AsyncLoadGameFromSlot("SurvivalRecord", ControllerId)
SAVE --> CTRL : 读档完成（校验激活令牌，旧页面结果丢弃）
CTRL -> CTRL : 比较并标记 bNewBest*
CTRL -> VIEW : SetResult（含纪录）
opt 有新纪录
  CTRL -> SAVE : AsyncSaveGameToSlot
end

alt bCanControlMatch（房主 / 单机）
  VIEW -> CTRL : RequestPlayAgain
  CTRL -> PS : ServerRequestPlayAgain [Server RPC]
  PS -> GM : 再次校验房主 → RequestPlayAgain\nServerTravel("?Restart")（无缝切图，统计不带入）
  VIEW -> CTRL : RequestReturnToLobby
  CTRL -> PS : ServerRequestReturnToLobby [Server RPC]
  PS -> GM : ReturnToLobby()（房间基类已有）
else
  VIEW -> VIEW : 显示"等待房主"
end
@enduml
```

## 8. 网络与复制

| 数据 | 所在 | 复制方式 | 说明 |
|---|---|---|---|
| Phase、开始/结束时间 | GameState | 属性 + OnRep | 时间用 `GetServerWorldTimeSeconds` 基准，客户端自行算倒计时，不每秒复制 |
| CurrentWave、NextWaveServerTime、AliveEnemyCount | WaveDirector 组件 | 属性 + OnRep | AliveEnemyCount 仅用于 HUD，变化频繁时可限频 |
| TeamLevel、TeamExperience | Progression 组件 | 属性 + OnRep | |
| Stats | PlayerState / GameState | FastArray | 增量复制 |
| LifeState、RespawnServerTime | PlayerState | 属性 + OnRep | |
| 横幅、提示类消息 | GameState | NetMulticast **Unreliable** | 丢了不影响正确性；需要可靠的状态一律走属性 |
| 出生预警 | GameplayCue | 由 Cue 系统复制 | |

不做客户端预测：所有玩法判定只在服务器；客户端只显示。

## 9. 调试与调参工具

成熟项目都会为玩法模式配套作弊命令和可视化调试，否则调波次和复活要反复打很久。

| 工具 | 内容 |
|---|---|
| 作弊命令（`UCC_SurvivalCheatExtension`，只在非 Shipping） | `Survival.SetWave N`、`Survival.SkipWave`、`Survival.AddXP N`、`Survival.KillAllEnemies`、`Survival.God`、`Survival.ReviveAll`、`Survival.EndMatch`；客户端输入时经 Server RPC 转发 |
| 日志分类 | `LogCCSurvival`：阶段切换、每波数量与档位、击杀归属、复活点选择；Verbose 打印每只怪的 SpawnParams |
| 屏幕调试 | `cc.Survival.Debug 1`：显示当前波、档位、存活数、队列长度、下一波倒计时、各玩家复活倒计时 |
| 调参 CVar | `cc.Survival.WaveIntervalScale`、`cc.Survival.SpawnCountScale`、`cc.Survival.XPScale`，只在开发构建生效，不写回配置 |
| 随机种子 | 每局生成种子写入日志；`?SurvivalSeed=` 可复现同一局的抽怪顺序 |
| 平衡数据 | 每波结束追加一行 CSV 到 `Saved/Survival/`：波次、人数、刷出数、击杀数、承伤、阵亡数、清场用时，用于画难度曲线 |

## 10. 性能预算

| 项 | v1 做法 | 以后需要时 |
|---|---|---|
| 同帧生成开销 | 分帧队列 | 对象池（复用 Character，重置 ASC 和 RuntimeData） |
| 场上数量 | `MaxAliveEnemies` 硬上限，默认 60 | Significance Manager 按距离降级 |
| 动画 | 引擎默认 | Animation Budget Allocator / URO |
| 网络 | Iris 默认 | 远处怪物降低复制频率 |
| 资源加载 | 软引用 + 预加载当前档和下一档 | |

## 11. 硬性规则对照

| 规则 | 本设计如何满足 |
|---|---|
| 1 Buff 只声明状态 | Survival 代码不认识任何 Buff；升级只调 `OfferBuffs(Pool)`；"经验 +20%"以后做成属性，由 Progression 读属性 |
| 2 Definition 不加玩法字段 | `ExperienceReward` 加在 `UCC_CharacterConfig`（角色配置），不是 Buff 定义 |
| 3 伤害走 ApplyDamage | 不新增伤害路径；击杀者从现有 Context 解析；回收怪物直接销毁，不走伤害 |
| 4 敌我只走统一查询 | 击杀归属用 `UCC_TeamStatics::IsHostile / GetEffectiveTeam` |
| 5 对称回收 | `ResumeAfterRevive` ↔ `ClearBuffs(OwnerDeath)`；统计事件订阅 ↔ Logout/EndMatch 退订；选卡保护 GE 施加 ↔ 移除；GameOver 时 `ClearOffers`、取消复活、清空刷怪队列 |
| 6 共享配置只读 | `UCC_SurvivalModeConfig` 只读；本波参数存在怪物实例的 `Transient` 字段上 |
| 7 钩子上下文注明合并方式 | 不新增钩子上下文；`FCC_DeathInfo`、`FCC_MatchMessage` 是只读通知 |

## 12. 边界情况与检查项

1. **同一帧多只怪死亡导致连升多级**：while 循环 + 三选一自带排队。
2. **死亡时正在选 Buff**：本轮保留；选择返回 `NoTarget`，界面提示"复活后可选"（复用 `Last Result`）。
3. **全灭瞬间有人复活定时器到时**：`OnRespawnTimer` 先检查 `Phase == Playing`；`EndMatch` 也会 `CancelAll`。
4. **GameOver 之后仍有死亡/伤害事件**（DoT、飞行中的箭）：GameMode 所有处理函数在非 `Playing` 时直接返回。
5. **玩家断线 / 房主断线**：普通玩家见 7.4；房主断线即会话结束，由现有房间系统处理。
6. **中途加入**（房主允许时）：`HandleStartingNewPlayer` 中在存活队友身边出生，`GrantCatchUpOffers` 补发 `TeamLevel - 1` 轮；全员阵亡时到达则以 Dead 状态等待结算。
7. **敌人锁定阵亡玩家**：包围系统已处理；**需确认** `ACC_AIController` 选目标时跳过 `!IsAlive()` 的目标。
8. **玩家死亡能力**：复用 Pawn，不能像 `UCC_Death`（敌人专用）那样销毁 Actor；**需确认玩家当前用哪个死亡能力**，并让它在结束时上报 DeathFinished。
9. **Invincible 是否生效**：**需确认**伤害 GE 已配置在目标带 `Status.Invincible` 时拒绝施加；没有的话在伤害执行里补一处通用检查（系统层，不违反规则 1）。
10. **刷怪点全部离玩家太近 / 都在屏幕内**：逐级放宽条件，选最远的刷怪器，不跳过本波。
11. **场上达到上限**：本波按剩余名额刷，记日志；回收的名额下一波补。
12. **单机暂停**：三选一打开时世界暂停，波次、刷怪队列、复活定时器同步暂停；回收计时也要用游戏时间而不是真实时间。
13. **升级 Buff 资产要求**：`BuffPool` 里的 GE 必须 `bRemoveOnDeath = false`，`UCC_SurvivalModeConfig::IsDataValid` 检查并警告。
14. **无缝切图再来一局**：`ACC_SurvivalPlayerState::CopyProperties` 不复制本局统计；ASC 上的升级 Buff 需在新局开始时清空（新地图会重新创建角色，确认 PlayerState 的 ASC 在 `CopyProperties` 后不保留 GE）。
15. **单机入口**：确认单机开始游戏使用的 GameMode 也切到 `ACC_SurvivalGameMode` 的蓝图子类。

## 13. 与成熟项目的对照

| 主题 | 参考做法 | 本项目选择 | 理由 |
|---|---|---|---|
| 死亡触发 | Lyra：`HealthSet` 广播 OutOfHealth（带 Instigator），`HealthComponent` 分 DeathStarted / Finished，死亡能力取消其他能力 | 采用 | 解决击杀者缺失和回调顺序问题 |
| 模式逻辑放哪 | Lyra：GameState 上挂组件，GameMode 只做生成规则 | 波次、经验放 GameState 组件；复活留在 GameMode | 有复制状态的放 GameState，纯服务器逻辑留 GameMode |
| 对局阶段 | ShooterGame/UT：`AGameMode::MatchState`；Lyra：阶段 = GameplayAbility | 自建枚举 | 房间基类是 GameModeBase；阶段只有三个 |
| 统计 | Lyra：PlayerState 的 `StatTags`（TagStack FastArray） | 采用 | 新增统计不改结构 |
| 消息 | Lyra：GameplayMessageRouter + Verb 消息复制 | 简化版：GameState 的 Unreliable 多播 | 不额外引入插件；只用于提示 |
| 复活 | Lyra：销毁旧 Pawn、`RestartPlayer` 生成新 Pawn，ASC 在 PlayerState 上保留 | **复用原 Pawn** | 本项目 `UCC_BuffComponent` 和 Behavior 在 Pawn 上，换 Pawn 会丢失 Behavior 状态；以后把 BuffComponent 移到 PlayerState 后可改为 Lyra 方式 |
| 死亡视角 | Lyra：死亡能力切换死亡相机模式 | 视角切到存活队友 | 俯视角游戏，看尸体意义不大 |
| 作弊命令 | Lyra：`CheatManager` 扩展 | 采用 | 调试必需 |
| 本地纪录 | Lyra：`ULocalPlayerSaveGame` 子类 | 采用 | 异步、按本地玩家、带版本号 |

## 14. 自动化测试（`GAS_Demo.Survival.*`）

| 测试 | 覆盖 |
|---|---|
| `Survival.Wave.TierLookup` | `FindTier` 边界（第 1 波、跨档、超过最后一档） |
| `Survival.Wave.Count` | `ComputeCount`、人数缩放、上限截断、队列中数量计入 |
| `Survival.Wave.Seed` | 同一种子抽怪顺序一致 |
| `Survival.Progression.LevelUp` | 一次大量经验连升多级、每级每人一轮、MaxLevel、中途加入补发 |
| `Survival.KillCredit` | 玩家击杀 / 怪物互杀 / 魅惑友军击杀 / 无击杀者 / 回收 |
| `Survival.Death.OutOfHealth` | 只广播一次；复活回血后可再次触发；非 GE 归零也触发 |
| `Survival.Respawn.Delay` | `ComputeDelay` 递增与封顶 |
| `Survival.Respawn.Revive` | 复活后 `bRemoveOnDeath=false` 的 Buff 恢复、Behavior 重新 Enable、Invincible 到期移除 |
| `Survival.Match.TeamWipe` | 全灭判定（含 Dying）、断线后判定、GameOver 后忽略事件与取消复活 |
| `Survival.Stats` | TagStack 累加、统计订阅与退订对称 |

需实机验证（功能测试地图 `L_SurvivalTest` + 多客户端 PIE）：分帧生成与预警表现、离屏刷怪、复活落点、观战视角、结算页与再来一局、单机暂停。

## 15. 实现顺序

| 步骤 | 内容 | 完成标志 |
|---|---|---|
| 1 | `OnOutOfHealth`、HandleDeath 防重入、DeathStarted/Finished、`UCC_GameEventSubsystem` | 日志打出每次死亡的 Victim / Killer |
| 2 | `ACC_SurvivalGameMode`（继承 `ACC_RoomGameMode`）、GameState / PlayerState、TagStack 统计、全灭 → GameOver、结算页、作弊命令骨架 | 单机死亡进结算页，显示击杀数 |
| 3 | 刷怪器 `SpawnOne` + 怪物 `SpawnParams` + `InitializeAttributes` 覆盖 + 人数缩放 GE | 作弊命令能刷出指定档位属性的怪 |
| 4 | `UCC_WaveDirectorComponent`：波次、分帧队列、预警、落点筛选、回收、HUD | 定时循环刷怪无明显卡顿 |
| 5 | `UCC_TeamProgressionComponent` 接入 `OfferBuffs`、选卡保护 | 击杀升级弹出三选一 |
| 6 | `UCC_RespawnComponent` + `Revive` + `ResumeAfterRevive` + 观战视角 | 联机阵亡后在队友身边复活，升级 Buff 仍在 |
| 7 | 平衡 CSV、调试显示、测试补齐、更新 `BuffSystem-Roadmap`、关闭第 12 节检查项 | 测试通过 |

## 16. 以后可能扩展（v1 不做）

- 精英 / Boss 波：`FCC_WaveTier` 增加 `SpecialWaves`（每 N 波替换怪物池并附加精英 GE）。
- 经验球拾取（参考已有 `CC_BuffPickup`）替代直接加经验。
- 三选一的稀有度、重抽、跳过、放逐（`BuffSelection.zh-CN.md` 已列为待扩展）。
- 对象池、Significance Manager。
- 把 `UCC_BuffComponent` 移到 PlayerState 后改为 Lyra 式"销毁并重新生成 Pawn"复活。
- 阶段增多时迁移到"阶段 = GameplayAbility"的阶段子系统。

## 17. 编辑器接线

已移到使用说明：[SurvivalMode-Usage.zh-CN.md](SurvivalMode-Usage.zh-CN.md)（接入清单、调参、调试、常见问题）。

## 18. 版本记录

**v2.1**（实现）：C++ 按 v2 实现，差异见第 0 节；新增第 17 节编辑器接线。


**v2**（对照 Lyra / ShooterGame 补充）

- 基类改为 `ACC_RoomGameMode`，复用无缝切图、准入与返回大厅；新增 `ACC_SurvivalGameState` / `ACC_SurvivalPlayerState` 子类，不改动现有 GameState / PlayerState。
- 死亡触发改为 AttributeSet 的 `OnOutOfHealth`，死亡分 Started / Finished 两段。
- 波次、经验改为 GameState 组件，各自持有复制数据。
- 统计改用 TagStack 容器，新增伤害、复活等统计项。
- 新增：人数缩放、分帧刷怪与出生预警、导航可达与离屏刷怪、卡怪回收、选卡期间保护、观战视角、复活无敌（复用 `Status.Invincible`）、中途加入追赶、再来一局、作弊命令与调试工具、平衡数据导出、网络复制表、性能预算、与成熟项目对照表。

**v1**：初版。
