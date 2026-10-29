# 文档目录

按系统分类。代码与系统的对应关系见根目录 [README](../README.md)。

| 文件夹 | 内容 |
|---|---|
| [Gameplay](#gameplay) | 角色数据、伤害、近战、刷怪、敌人、生存模式 |
| [Buff](#buff) | Buff 系统设计与实现、三选一、召唤 |
| [AI](#ai) | 群体战斗 AI、寻敌 |
| [UI](#ui) | UI 框架与各界面 |
| [Network](#network) | 联机房间 |
| [Audio](#audio) | 音效系统 |
| [Debug](#debug) | 调试开关 |
| [Rendering](#rendering) | 渲染研究笔记 |
| [Archive](#archive) | 已被取代的旧方案，仅作历史记录 |

## Gameplay

| 文档 | 说明 |
|---|---|
| [CharacterData](Gameplay/CharacterData.md) | 角色默认配置（`CC_CharacterConfig`）与运行时数据 |
| [DamageSystem](Gameplay/DamageSystem.zh-CN.md) | 伤害与治疗链路 |
| [Combat-MeleeEvents](Gameplay/Combat-MeleeEvents.zh-CN.md) | 近战判定通知与命中事件 |
| [EnemySpawner](Gameplay/EnemySpawner.zh-CN.md) | 场景怪物生成点 |
| [PolyartMeleeEnemies](Gameplay/PolyartMeleeEnemies.zh-CN.md) | Polyart 近战敌人资产 |
| [EnemyArcherAnimGraph](Gameplay/EnemyArcherAnimGraph.zh-CN.md) | 弓兵动画蓝图（附 [结构图](Gameplay/EnemyArcherAnimGraph-Diagram.html)） |
| [SurvivalMode](Gameplay/SurvivalMode.zh-CN.md) | 生存模式设计（PlantUML 源文件在 [Survival/](Gameplay/Survival/)） |
| [SurvivalMode-Usage](Gameplay/SurvivalMode-Usage.zh-CN.md) | 生存模式接入、调参与排错 |

## Buff

建议阅读顺序：设计原则 → 架构 → 结构图 → 实例 → 案例手册 → 现状与待办。

| 文档 | 说明 |
|---|---|
| [BuffSystem](Buff/BuffSystem.zh-CN.md) | 设计原则（必读） |
| [BuffSystem-Architecture](Buff/BuffSystem-Architecture.zh-CN.md) | 目标架构 |
| [BuffSystem-Diagrams](Buff/BuffSystem-Diagrams.zh-CN.md) | 已实现部分的结构图、序列图、类图（Mermaid 源文件在 [Diagrams/](Buff/Diagrams/)） |
| [FourExamples](Buff/FourExamples.zh-CN.md) | 四种 Buff 的实际实现 |
| [BuffSystem-Cookbook](Buff/BuffSystem-Cookbook.zh-CN.md) | 案例手册：分裂箭、爆炸箭、冻结、魅惑、反弹等 |
| [BuffSystem-Roadmap](Buff/BuffSystem-Roadmap.zh-CN.md) | 基础设施现状与待办 |
| [BuffSelection](Buff/BuffSelection.zh-CN.md) | Buff 三选一 |
| [NewBuffOffers](Buff/NewBuffOffers.zh-CN.md) | NewBuff 选项池 |
| [SummonAlly](Buff/SummonAlly.zh-CN.md) | 召唤随从 Buff |

## AI

| 文档 | 说明 |
|---|---|
| [AI-Combat](AI/AI-Combat.zh-CN.md) | 群体战斗 AI v3（现行设计） |
| [AI-Targeting](AI/AI-Targeting.zh-CN.md) | 寻敌逻辑 |

## UI

| 文档 | 说明 |
|---|---|
| [UIArchitecture](UI/UIArchitecture.zh-CN.md) | View / Controller / Model 分层（先读这篇） |
| [UIFramework](UI/UIFramework.zh-CN.md) | Lyra 风格 UI 框架底层 |
| [LyraUI-Phase1](UI/LyraUI-Phase1.zh-CN.md) | LocalPlayer 驱动的 UI 生命周期 |
| [UIWidgetBindings](UI/UIWidgetBindings.zh-CN.md) | 控件命名与绑定规范 |
| [CharacterScreen](UI/CharacterScreen.zh-CN.md) | 角色面板（PlantUML 图在 [Diagrams/](UI/Diagrams/)） |
| [CombatMain-CharacterScreen-Setup](UI/CombatMain-CharacterScreen-Setup.zh-CN.md) | 战斗主界面与角色面板的蓝图接入步骤 |
| [ToastNotification](UI/ToastNotification.zh-CN.md) | 提示弹窗 |
| [WorldIndicator](UI/WorldIndicator.zh-CN.md) | 世界指示器：头顶血条、伤害数字 |
| [LANMenuWidgetSetup](UI/LANMenuWidgetSetup.zh-CN.md) | 主菜单与局域网页面接入 |
| [SettingsUI-Layout](UI/SettingsUI-Layout.md) | 设置界面布局方案（附 [效果图](UI/SettingsUI-Layout-Mockup.html)） |

## Network

| 文档 | 说明 |
|---|---|
| [OnlineRoom](Network/OnlineRoom.zh-CN.md) | 创建、查找、进入房间与大厅 |

## Audio

| 文档 | 说明 |
|---|---|
| [AudioConfig](Audio/AudioConfig.zh-CN.md) | 全局音效配置 |
| [AudioSystem-Handoff](Audio/AudioSystem-Handoff.zh-CN.md) | 音效系统交接：使用、挑选台、Python 导入 |

## Debug

| 文档 | 说明 |
|---|---|
| [DebugCVars](Debug/DebugCVars.zh-CN.md) | 调试开关（`cc.Debug.*` 控制台变量） |

## Rendering

| 文档 | 说明 |
|---|---|
| [ForestWorld-FrameAnalysis](Rendering/ForestWorld-FrameAnalysis.zh-CN.md) | 《林间小世界》截帧解析：4 个核心 Pass |

## Archive

| 文档 | 说明 |
|---|---|
| [AI-Engagement](Archive/AI-Engagement.zh-CN.md) | 怪物包围寻路 v2（槽位 + 令牌），已被 AI-Combat v3 取代 |
| [AI-Engagement-Usage](Archive/AI-Engagement-Usage.zh-CN.md) | 同上方案的使用说明 |
| [PawnData](Archive/PawnData.md) | 旧版 `CC_PawnData`，已被 CharacterData 取代 |

## 新增文档的约定

- 放进对应系统的文件夹；跨系统的放在主要归属的那一个。
- 图的源文件（PlantUML / Mermaid）放在该文件夹下的 `Diagrams/` 子目录。
- 方案被取代后移到 `Archive/`，并在文首注明取代它的文档。
- 新增或移动后更新本页。
