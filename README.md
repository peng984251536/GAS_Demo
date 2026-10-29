# GAS_Demo

基于 **Unreal Engine 5.6 + Gameplay Ability System（GAS）** 的 C++ 动作战斗 Demo。

玩法：近战 / 弓箭战斗 + 肉鸽式 **Buff 三选一** + **生存模式**（波次刷怪、共享经验、倒计时复活），支持局域网联机（Listen Server）。同屏目标为 100～150 只小怪的多对多混战。

本文按"游戏系统"介绍代码组织，方便从目录找到对应实现。每个系统都给出 **代码位置** 和 **详细文档**。全部文档按系统分类放在 `Docs/` 下，索引见 [Docs/README](Docs/README.md)。

> 头文件在 `Source/GAS_Demo/Public/<模块>/`，实现在 `Source/GAS_Demo/Private/<模块>/`，两边目录结构一一对应。下文只列 Public 路径。
> 所有项目类以 `CC_` 为前缀（`UCC_` / `ACC_` / `FCC_`）。

---

## 目录

1. [快速导航](#快速导航)
2. [源码目录总览](#源码目录总览)
3. [GAS 能力系统](#1-gas-能力系统)
4. [输入与连招](#2-输入与连招)
5. [伤害与治疗](#3-伤害与治疗)
6. [Buff 系统](#4-buff-系统)
7. [Buff 三选一](#5-buff-三选一)
8. [角色、投射物与生成点](#6-角色投射物与生成点)
9. [阵营（敌我判断）](#7-阵营敌我判断)
10. [AI 系统](#8-ai-系统)
11. [生存模式](#9-生存模式)
12. [联机房间](#10-联机房间)
13. [UI 系统](#11-ui-系统)
14. [音效系统](#12-音效系统)
15. [测试与工具](#13-测试与工具)
16. [开发约定](#开发约定)
17. [运行项目](#运行项目)

---

## 快速导航

| 想了解 | 从这里开始读 | 文档 |
|---|---|---|
| 角色怎么拿到技能、技能怎么激活 | `CC_AbilitySystemComponent`、`CC_GameplayAbility` | [CharacterData](Docs/Gameplay/CharacterData.md) |
| 攻击怎么出伤害 | `UCC_DamageStatics::ApplyDamage` → `CC_DamageExecution` → `CC_AttributeSet` | [DamageSystem](Docs/Gameplay/DamageSystem.zh-CN.md) |
| Buff 怎么生效、怎么写新 Buff | `UCC_BuffComponent`、`UCC_BuffBehavior` | [BuffSystem](Docs/Buff/BuffSystem.zh-CN.md) |
| 升级后三选一 | `UCC_BuffSelectionComponent`、`UCC_BuffOfferPool` | [BuffSelection](Docs/Buff/BuffSelection.zh-CN.md) |
| 怪物怎么选目标、怎么围攻 | `ACC_AIController`、`CC_TargetScoring` | [AI-Combat](Docs/AI/AI-Combat.zh-CN.md) |
| 波次 / 经验 / 复活 | `ACC_SurvivalGameMode`、`CC_WaveDirectorComponent` | [SurvivalMode](Docs/Gameplay/SurvivalMode.zh-CN.md) |
| 界面怎么打开、数据怎么绑定 | `UCC_UIManagerSubsystem`、`UCC_UIController` | [UIArchitecture](Docs/UI/UIArchitecture.zh-CN.md) |
| 打开调试画线 / 屏幕信息 | `CC_DebugCVars` | [DebugCVars](Docs/Debug/DebugCVars.zh-CN.md) |
| 联机建房、加入 | `UCC_OnlineRoomSubsystem` | [OnlineRoom](Docs/Network/OnlineRoom.zh-CN.md) |

---

## 源码目录总览

```text
Source/GAS_Demo/Public/
├── AbilitySystem/      GAS 核心：ASC、技能、属性、GE、GameplayCue、AbilityTask
├── Input/              输入配置与输入缓冲
├── Animation/Notifies/ 连招窗口、近战判定、放箭、脚步/音效通知
├── Damage/             伤害入口、计算、公式、伤害上下文
├── Components/         角色组件：连招状态、Buff 管理、三选一、召唤物 Buff 共享
├── Data/               数据资产：角色配置、动作集、Buff 规则与选项池
├── Actors/             角色、投射物、拾取物、刷怪点
├── AI/                 AI 控制器、选目标、交战登记、编队、Crowd 避让、行为树节点
├── Game/               GameState、房间框架、生存模式
├── Player/             玩家 Controller / PlayerState
├── Subsystems/         全局事件、联机房间
├── UI/                 UI 框架与各界面
├── Audio/              音效配置与运行时
├── GameplayTags/       原生 GameplayTag 声明
└── Utils/              蓝图函数库、阵营查询
Variant_TwinStick/ Variant_Strategy/   UE 模板自带示例，非本项目核心
```

---

## 1. GAS 能力系统

角色的技能、属性、状态全部走 GAS。

| 内容 | 位置 | 说明 |
|---|---|---|
| ASC | [`AbilitySystem/CC_AbilitySystemComponent`](Source/GAS_Demo/Public/AbilitySystem/CC_AbilitySystemComponent.h) | 项目 ASC，提供按标签查询能力状态等接口（供行为树调用） |
| 技能基类 | [`AbilitySystem/Abilities/`](Source/GAS_Demo/Public/AbilitySystem/Abilities) | `CC_GameplayAbilityBase`（输入配置）、`CC_GameplayAbility`（蒙太奇播放） |
| 战斗技能 | `Abilities/Mechanics/` | `CC_Primary` 普攻、`CC_BowShoot` 射箭、`CC_Dash` 冲刺、`CC_Dodge` 闪避、`CC_SummonAlly` 召唤随从 |
| AI 行为技能 | `Abilities/Logic/` | `CC_FindPlayerTarget` / `CC_FindRangedTarget` 寻敌、`CC_FollowTarget` 追击、`CC_KeepDistance` 保持距离、`CC_ListenToHealth` |
| 反应技能 | `Abilities/Reaction/` | `CC_HitReact` 受击、`CC_Death` 死亡 |
| 攻击意图 | `Abilities/Intent/CC_AttackIntend` | 攻击意图 |
| 属性集 | [`AbilitySystem/Attributes/CC_AttributeSet`](Source/GAS_Demo/Public/AbilitySystem/Attributes/CC_AttributeSet.h) | 生命、法力、战斗属性；消费 `Damage` / `Healing` 中间属性完成扣血和治疗 |
| GameplayEffect | [`AbilitySystem/Effects/`](Source/GAS_Demo/Public/AbilitySystem/Effects) | 伤害 / 治疗 GE、受击、死亡、连招窗口，以及 `Buff/` 下的各种 Buff GE |
| GameplayCue | [`AbilitySystem/GameplayCues/`](Source/GAS_Demo/Public/AbilitySystem/GameplayCues) | 伤害飘字、命中特效 |
| AbilityTask | `AbilitySystem/Tasks/CC_AttributeChangeTask` | 监听属性变化 |
| 标签 | [`GameplayTags/CC_Tags`](Source/GAS_Demo/Public/GameplayTags/CC_Tags.h) | 项目原生 GameplayTag |

文档：[CharacterData](Docs/Gameplay/CharacterData.md)

## 2. 输入与连招

| 内容 | 位置 | 说明 |
|---|---|---|
| 输入配置 | [`Input/`](Source/GAS_Demo/Public/Input) | `GASInputConfig` 输入行为 → 技能映射；`FBufferedAbilityInput` 输入缓冲（后输入覆盖先输入） |
| 连招状态 | [`Components/ActionComponent`](Source/GAS_Demo/Public/Components/ActionComponent.h) | 当前第几段、是否在输入窗口内、当前动作 |
| 动作数据 | [`Data/AbilityData/`](Source/GAS_Demo/Public/Data/AbilityData) | `CombatActionSet`：一个角色 / 武器 / 怪物的动作目录，换 ActionSet 即换一套连招 |
| 动画通知 | [`Animation/Notifies/`](Source/GAS_Demo/Public/Animation/Notifies) | `ANS_ComboStart` / `ANS_ComboWindow` / `ANS_Comboing` 连招窗口；`AnimNotify_MeleeHitBox` 近战判定与出伤帧；`AnimNotify_BowRelease` 放箭 |

文档：[Combat-MeleeEvents](Docs/Gameplay/Combat-MeleeEvents.zh-CN.md)

## 3. 伤害与治疗

链路：**`UCC_DamageStatics::ApplyDamage`** → 伤害 GE → `CC_DamageExecution`（计算）→ `CC_AttributeSet`（扣血、发送 `Event.Combat.DamageDealt / DamageTaken`）→ GameplayCue 飘字。

| 内容 | 位置 |
|---|---|
| 唯一伤害入口 | [`Damage/CC_DamageStatics`](Source/GAS_Demo/Public/Damage/CC_DamageStatics.h) |
| 计算与公式 | `Damage/CC_DamageExecution`、`CC_DamageFormula`、`CC_DamageSettings` |
| 伤害上下文 / 来源标签 | `Damage/CC_GameplayEffectContext`、`CC_DamageTypes`、`CC_DamageTags` |
| 伤害 / 治疗 GE | `AbilitySystem/Effects/Damage/` |

> 所有伤害必须经过 `ApplyDamage` 并填写 `Damage.Source.*` 来源标签，否则"造成伤害后"类 Buff 不会触发。

文档：[DamageSystem](Docs/Gameplay/DamageSystem.zh-CN.md)

## 4. Buff 系统

核心原则：**Buff 只声明"角色处于什么状态、挂着什么规则"，机制由各系统实现。** 技能、AI、伤害代码里不出现针对某个具体 Buff 的判断。

Buff 分三层：

| 层 | 实现方式 | 例子 |
|---|---|---|
| 数值层 | 普通 GE Modifier | 攻击力 +20、生命再生 |
| 状态层 | GE 授予标签，由系统消费标签 | 霸体（`Status.SuperArmor`）、魅惑（`Status.Charmed`） |
| 规则层 | GE 上挂 `UCC_BuffBehavior` 子类 | 分裂箭、连续两箭、命中附加魅惑 |

| 内容 | 位置 | 说明 |
|---|---|---|
| 运行时管理 | [`Components/CC_BuffComponent`](Source/GAS_Demo/Public/Components/CC_BuffComponent.h) | 每个角色一份；观察 ActiveGE，管理 Behavior 实例、钩子和客户端 UI 快照 |
| 规则配置 | [`Data/Buff/CC_BuffRulesComponent`](Source/GAS_Demo/Public/Data/Buff/CC_BuffRulesComponent.h) | GE 组件，保存 Behavior 模板和死亡清理策略（只读） |
| 规则扩展点 | [`Data/Buff/CC_BuffBehavior`](Source/GAS_Demo/Public/Data/Buff/CC_BuffBehavior.h) | 状态机 `Uninitialized → Disabled ⇄ Enabled → Finished`，通过基类工具登记资源以便自动回收 |
| 具体 Behavior | `Data/Buff/CC_BuffBehavior_SplitArrow` 等 | 分裂箭等规则实现 |
| Buff GE 示例 | [`AbilitySystem/Effects/Buff/CC_GE_NewOffers`](Source/GAS_Demo/Public/AbilitySystem/Effects/Buff/CC_GE_NewOffers.h) | 永久强攻、生命再生、十秒霸体、魅惑攻击、分裂箭、连续两箭等 |
| 标签 / 类型 / UI 数据 | `Data/Buff/CC_BuffTags`、`CC_BuffTypes`、`CC_BuffUIData`、`CC_BuffDebug` | |
| 投射物交互 | `Actors/Projectiles/CC_ProjectileInteractions` | 反弹敌方箭等通用资格，不依赖具体 Buff |
| 召唤随从 | `Abilities/Mechanics/CC_SummonAlly`、`Components/CC_SummonBuffShareComponent` | 召唤同阵营随从，随从同步复制召唤者的 Buff |

文档（建议按顺序读）：
[设计原则](Docs/Buff/BuffSystem.zh-CN.md) →
[架构](Docs/Buff/BuffSystem-Architecture.zh-CN.md) →
[结构图 / 序列图 / 类图](Docs/Buff/BuffSystem-Diagrams.zh-CN.md) →
[四种 Buff 实例](Docs/Buff/FourExamples.zh-CN.md) →
[案例手册](Docs/Buff/BuffSystem-Cookbook.zh-CN.md) →
[现状与待办](Docs/Buff/BuffSystem-Roadmap.zh-CN.md)；
另见 [召唤 Buff](Docs/Buff/SummonAlly.zh-CN.md)、新增 Buff 流程 [`.claude/skills/add-buff`](.claude/skills/add-buff/SKILL.md)

## 5. Buff 三选一

服务器从选项池随机抽 3 个，玩家选一个，然后走普通的 `UCC_BuffComponent::ApplyBuff`。联机时每个玩家各自抽、各自选。升级只增加待选次数并在主界面显示红点，点按钮才打开选择界面。

| 内容 | 位置 |
|---|---|
| 选择逻辑（服务器） | [`Components/CC_BuffSelectionComponent`](Source/GAS_Demo/Public/Components/CC_BuffSelectionComponent.h)、`CC_BuffSelectionTypes` |
| 选项池 | `Data/Buff/CC_BuffOfferPool`、`CC_BuffBehavior_OfferRules` |
| 场景拾取 | `Actors/Pickups/CC_BuffPickup` |
| 界面 | `UI/BuffSelection/`（`CC_BuffSelectionScreen`、`CC_BuffOfferCardWidget`、`CC_BuffSelectionController`） |

文档：[BuffSelection](Docs/Buff/BuffSelection.zh-CN.md)、[NewBuff 选项池](Docs/Buff/NewBuffOffers.zh-CN.md)

## 6. 角色、投射物与生成点

| 内容 | 位置 | 说明 |
|---|---|---|
| 角色 | [`Actors/Character/`](Source/GAS_Demo/Public/Actors/Character) | `CC_BaseCharacter` 基类、`CC_PlayerCharacter` 玩家、`CC_EnemyCharacter` 敌人 / 随从 |
| 角色数据 | [`Data/`](Source/GAS_Demo/Public/Data) | `CC_CharacterConfig` 默认配置（数据资产）、`CC_CharacterRuntimeData` 每个角色的运行时状态 |
| 投射物 | [`Actors/Projectiles/`](Source/GAS_Demo/Public/Actors/Projectiles) | `CC_ArrowProjectile` 箭、`CC_ProjectileFireContext` 放箭上下文（Buff 可修改箭数、散射等） |
| 刷怪点 | `Actors/Spawners/CC_EnemySpawner`、`Data/CC_EnemySpawnTypes` | 场景内摆放的怪物生成点 |
| 玩家 | [`Player/`](Source/GAS_Demo/Public/Player) | `CC_PlayerController`、`CC_PlayerState` |
| 全局事件 | `Subsystems/CC_GameEventSubsystem` | 角色死亡等只读通知，发送方不关心谁在监听 |

文档：[CharacterData](Docs/Gameplay/CharacterData.md)、[EnemySpawner](Docs/Gameplay/EnemySpawner.zh-CN.md)、[Polyart 近战敌人](Docs/Gameplay/PolyartMeleeEnemies.zh-CN.md)

## 7. 阵营（敌我判断）

[`Utils/Team/CC_TeamStatics`](Source/GAS_Demo/Public/Utils/Team/CC_TeamStatics.h) 是**全项目唯一的敌我判断入口**（`IsHostile`）。AI 选目标、近战判定、投射物命中、范围伤害都调用它；魅惑等状态通过这里改变敌我关系。

## 8. AI 系统

群体战斗 AI v3（骑砍式三层：个体自由交战 + 编队接口预留），目标规模同屏 100～150 只。

| 内容 | 位置 | 说明 |
|---|---|---|
| AI 控制器 | [`AI/CC_AIController`](Source/GAS_Demo/Public/AI/CC_AIController.h) | 目标选择与切换 |
| 选目标评分 | `AI/Targeting/CC_TargetScoring` | 按距离、拥挤度、回敬等打分，分低者优先 |
| 交战登记 | `AI/Engagement/CC_EngagementSubsystem` | 记录"谁在打谁"，用于计算拥挤与回敬 |
| 编队 | `AI/Squad/CC_SquadTypes` | 编队命令：自由交战 / 冲锋 / 跟随（Hold、Retreat 预留） |
| 避让 | `AI/Crowd/` | Detour Crowd；`CC_PlayerCrowdAgentComponent` 让玩家作为障碍参与避让 |
| 行为树节点 | `AI/BehaviorTree/` | `BTTask_CC_ActivateAbility` 激活技能、`BTDecorator_CC_InAttackRange` |
| 工具 | `AI/CC_CombatRangeStatics`、`AI/CC_MoveTaskWatchdog` | 攻击距离；移动类技能卡死时的看门狗 |
| 寻敌 / 追击技能 | `AbilitySystem/Abilities/Logic/` | 见第 1 节 |

文档：[AI-Combat（现行设计）](Docs/AI/AI-Combat.zh-CN.md)、[AI-Targeting](Docs/AI/AI-Targeting.zh-CN.md)。`AI-Engagement*.zh-CN.md` 为已被取代的 v2 方案，仅作历史记录。

## 9. 生存模式

地图按波次定时刷怪，怪物随波次变强；全队共享经验，升级时每人各自三选一；阵亡后倒计时在存活队友身边复活；全员阵亡则结算。

| 内容 | 位置 | 说明 |
|---|---|---|
| 规则 | [`Game/Survival/CC_SurvivalGameMode`](Source/GAS_Demo/Public/Game/Survival/CC_SurvivalGameMode.h) | 模式入口 |
| 状态 | `CC_SurvivalGameState`、`CC_SurvivalPlayerState` | |
| 波次调度 | `CC_WaveDirectorComponent` | 挂在 GameState 上，落点预警后生成怪物 |
| 共享经验 | `CC_TeamProgressionComponent` | 累加经验、判定升级、给每个玩家排一轮三选一 |
| 复活 | `CC_RespawnComponent` | 倒计时 → 选随机存活队友 → 找可达落点 → `Revive`（复用原 Pawn，Buff 保留） |
| 配置 / 统计 / 存档 | `CC_SurvivalModeConfig`、`CC_StatTagStack`、`CC_SurvivalSaveGame`、`CC_SurvivalTags`、`CC_SurvivalTypes` | |
| 界面 | `UI/Survival/CC_SurvivalResultController`、`UI/Toast/CC_SurvivalToastRelay` | 结算页、波次 / 升级 / 队友倒下提示 |

文档：[SurvivalMode 设计](Docs/Gameplay/SurvivalMode.zh-CN.md)、[SurvivalMode 使用说明](Docs/Gameplay/SurvivalMode-Usage.zh-CN.md)

## 10. 联机房间

Listen Server + OnlineSubsystem Session + 联机大厅。

| 内容 | 位置 |
|---|---|
| 房间子系统与类型 | [`Subsystems/`](Source/GAS_Demo/Public/Subsystems)：`CC_OnlineRoomSubsystem`、`CC_RoomSessionBackend`、`CC_RoomTypes` |
| OSS 适配器 | `Private/Subsystems/CC_OnlineSubsystemRoomBackend`（仅 Private） |
| 房间 GameMode / GameState 等 | `Game/CC_RoomFramework`、`Game/CC_GameState` |
| 大厅界面 | `UI/MainMenu/CC_MultiplayerScreen*`、`CC_MultiplayerRoomItemWidget` |

文档：[OnlineRoom](Docs/Network/OnlineRoom.zh-CN.md)、[主菜单和局域网页面接入](Docs/UI/LANMenuWidgetSetup.zh-CN.md)

## 11. UI 系统

参考 Lyra 的分层 UI，基于 CommonUI：

```text
GameInstance → UIManager → UIPolicy → 每个 LocalPlayer 一个 RootLayout → 四个 CommonUI 层栈（Game / Menu / Modal / 通知）
页面内部：View（Widget 蓝图）← Controller（业务协调）← Model（展示快照）
```

| 内容 | 位置 | 说明 |
|---|---|---|
| 框架 | [`UI/Framework/`](Source/GAS_Demo/Public/UI/Framework) | `CC_UIManagerSubsystem`、`CC_UIPolicy`、`CC_RootLayout`、`CC_ActivatableWidget`、`CC_AsyncShowScreen`、`CC_ButtonBase` |
| 业务分层 | `UI/Framework/CC_UIController`、`CC_UIModel` | Controller 管订阅和用户意图；Model 只存展示状态，不持有控件 |
| 主菜单 / 大厅 | `UI/MainMenu/` | |
| 战斗主界面 | `UI/Combat/` | `CC_CombatMainWidget`（血蓝、波次、经验、击杀、三选一红点）、`CC_PlayerVitalsWidget` |
| 角色面板 | `UI/Character/` | 左侧属性、右侧当前 Buff 及剩余时间 |
| Buff 栏 / 三选一 | `UI/Buff/`、`UI/BuffSelection/` | |
| 提示弹窗 | `UI/Toast/` | 不抢输入、自动消失的通知层提示 |
| 世界指示器 | `UI/WorldIndicator/`、`UI/WorldOverlay/` | 所有跟随世界位置的屏幕 UI 共用一个显示层 |
| 头顶血条 / 伤害数字 | `UI/HealthBar/`、`UI/DamageText/` | 世界指示器的两个数据源 |

文档：
[UIArchitecture](Docs/UI/UIArchitecture.zh-CN.md)、
[UIFramework](Docs/UI/UIFramework.zh-CN.md)、
[LyraUI-Phase1](Docs/UI/LyraUI-Phase1.zh-CN.md)、
[控件命名与绑定](Docs/UI/UIWidgetBindings.zh-CN.md)、
[角色面板](Docs/UI/CharacterScreen.zh-CN.md)、
[CombatMain 与角色面板接入](Docs/UI/CombatMain-CharacterScreen-Setup.zh-CN.md)、
[Toast](Docs/UI/ToastNotification.zh-CN.md)、
[WorldIndicator](Docs/UI/WorldIndicator.zh-CN.md)；
框架源码说明见 [`Private/UI/Framework/README.zh-CN.md`](Source/GAS_Demo/Private/UI/Framework/README.zh-CN.md)

## 12. 音效系统

| 内容 | 位置 | 说明 |
|---|---|---|
| 配置 | [`Audio/`](Source/GAS_Demo/Public/Audio)：`CC_AudioConfig`、`CC_AudioSettings` | 全局音效数据资产，在项目设置 → CC Audio 中指定 |
| 运行时 | `Audio/CC_AudioSubsystem`、`CC_AudioLibrary` | 预加载配置，按事件规则选出本次播放的变体、音量、音高 |
| 动画触发 | `Animation/Notifies/AnimNotify_PlayAudio`、`AnimNotify_Footstep` | |
| 挑选工具 | `Tools/AudioPicker/`、`Scripts/*audio*.py` | 音效挑选台与导入 UE 的 Python 脚本 |

文档：[AudioConfig](Docs/Audio/AudioConfig.zh-CN.md)、[音效系统交接](Docs/Audio/AudioSystem-Handoff.zh-CN.md)

## 13. 测试与工具

- **自动化测试**：[`Private/Tests/`](Source/GAS_Demo/Private/Tests)，覆盖 Buff、三选一、伤害、死亡、阵营、选目标、编队、召唤、生存模式、房间、Toast、UI 生命周期、世界指示器等。在编辑器 Session Frontend → Automation 中按 `GAS_Demo.*` 运行。
- **调试开关**：画线、屏幕调试文字统一由 `cc.Debug.*` 控制台变量控制，默认关闭，见 [DebugCVars](Docs/Debug/DebugCVars.zh-CN.md)。
- **规则检查**：[`Tools/HarnessChecks/`](Tools/HarnessChecks)，`check_buff_rules.sh` 检查 Buff / 伤害 / 阵营的硬性规则。
- **编辑器脚本**：[`Scripts/`](Scripts)，资产配置、UI 布局生成、材质与动画检查等 UE Python 脚本；`BuildAndLogErrors.cmd` 编译并输出错误。
- **其他工具**：`Tools/AutoClicker/`、`Tools/AudioPicker/`。

---

## 开发约定

完整规则见 [CLAUDE.md](CLAUDE.md)，要点：

1. Buff 只声明状态和规则，系统代码里不写 `HasBuff(某个 Buff)` 这类判断。
2. 所有伤害经过 `UCC_DamageStatics::ApplyDamage` 并填写来源标签。
3. 敌我判断只用 `UCC_TeamStatics::IsHostile`。
4. Buff 授予的一切必须能对称收回（抑制、叠层、死亡、移除都要覆盖）。
5. Definition、Behavior 模板、DataAsset 在运行时只读，不存实例状态。

## 运行项目

本仓库只包含 C++ 源码、配置、文档、测试和工具源码，**不包含 `Content/` 与 `SourceArt/`**（蓝图、地图、模型、动画、贴图等）。缺少这些资源时无法直接运行完整游戏，但可以阅读和编译代码。

1. 安装 Unreal Engine 5.6 和对应的 C++ 开发工具。
2. 将完整项目的 `Content/` 和需要的 `SourceArt/` 复制到项目目录。
3. 为 `GAS_Demo.uproject` 生成项目文件，编译后打开。
