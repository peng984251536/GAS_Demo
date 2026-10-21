# 角色面板

角色面板分左右两栏：左侧是角色属性（名字、生命、法力，以及可配置的属性列表），右侧是当前持有的 Buff，每条显示名称、效果描述和剩余时间。

它沿用 [UIArchitecture.zh-CN.md](UIArchitecture.zh-CN.md) 的 View / Controller / 展示 Model 分层，Buff 数据直接取自 Buff 系统（[BuffSystem-Architecture.zh-CN.md](BuffSystem-Architecture.zh-CN.md)）：从 `UCC_BuffComponent` 的复制快照读取列表，从 GE 上的 `UCC_BuffUIData` 读取显示信息。面板只读，不施加、不移除任何 Buff，也不认识任何具体 Buff。

## 图

PlantUML 源文件和渲染图在 `Docs/Diagrams/`：

| 图 | 内容 |
| --- | --- |
| [CharacterScreen_Architecture.puml](Diagrams/CharacterScreen_Architecture.puml) | 架构图：类、所有权、数据来源，标出哪些是 WBP |
| [CharacterScreen_Sequence_Open.puml](Diagrams/CharacterScreen_Sequence_Open.puml) | 时序图 1：打开、绑定数据源、首次推送、关闭 |
| [CharacterScreen_Sequence_Update.puml](Diagrams/CharacterScreen_Sequence_Update.puml) | 时序图 2：属性变化、Buff 增删和叠层（服务器 → FastArray → 客户端）、倒计时、换 Pawn |

## 类

| 类 | 层 | 职责 |
| --- | --- | --- |
| `UCC_CharacterScreenWidget` | View | 页面基类，WBP 继承它做布局；按版本号分别转发属性和 Buff 变化；可选地在 `BuffList` 中自动生成条目 |
| `UCC_CharacterBuffEntryWidget` | View | 单条 Buff；每帧按服务器时钟刷新倒计时；子控件都是可选绑定 |
| `UCC_CharacterScreenController` | Controller | 跟随当前 Pawn 绑定 ASC 属性和 `UCC_BuffComponent::OnBuffsChanged`，转换为展示数据 |
| `UCC_CharacterScreenModel` | 展示 Model | 保存名字、`FCC_PlayerVitals`、属性列表、Buff 列表，以及两个版本号；写入口只对 Controller 开放 |
| `CC_CharacterScreenTypes.h` | 数据结构 | `FCC_CharacterStatConfig`、`FCC_CharacterStatEntry`、`FCC_CharacterBuffEntry`，以及剩余时间计算 |

文件都在 `Source/GAS_Demo/{Public,Private}/UI/Character/`。

## 数据从哪里来

| 面板内容 | 来源 | 何时刷新 |
| --- | --- | --- |
| 名字 | PlayerState 的玩家名 | 随属性一起 |
| 生命、法力 | ASC：Health / MaxHealth / Mana / MaxMana | 属性变化委托 |
| 属性列表 | ASC：Controller 的 `DisplayedStats` 配置 | 属性变化委托 |
| Buff 列表 | `UCC_BuffComponent::GetActiveBuffs()` + GE 上的 `UCC_BuffUIData` | `OnBuffsChanged`（服务器本地广播，客户端由 FastArray 复制后广播） |
| 剩余时间 | 条目上的 `EndServerTime` − `GameState::GetServerWorldTimeSeconds()` | 条目每帧自己算，不经过 Model |

Buff 的过滤规则与 `CC_BuffBarWidget` 相同：GE 上没有 `UCC_BuffUIData`，或勾选了 `bHiddenInUI` 的不显示。列表顺序保持 BuffComponent 的顺序，也就是施加先后。

剩余时间与 `UCC_BuffComponent::GetRemainingTime` 用同一算法。单机打开面板时世界暂停，服务器时钟冻结，倒计时一起停；联机时照常走。

## 可配置项（Controller 蓝图默认值）

需要改配置时，创建一个继承 `CC_CharacterScreenController` 的蓝图，在页面 WBP 的 `Controller Class` 里选它。

- **Displayed Stats**：左侧属性列表，每行包括属性、显示名、格式（Integer / Decimal / Percent）。默认依次为攻击力、防御、暴击率、暴击伤害、移动速度，后三项按百分比显示（例如暴击倍率 1.5 显示为 150%）。生命和法力固定显示，不在这个列表里。
- **Debuff Tag**：可选。带有该标签（资产标签或授予标签）的 Buff 标记为 `bIsDebuff`，用于配色。项目里目前还没有区分增减益的标签；留空时全部视为增益。

## WBP 接线（布局由你负责）

**页面 `WBP_CharacterScreen`**

1. 父类选 `CC_CharacterScreenWidget`。`Controller Class` 默认已经是 `CC_CharacterScreenController`。
2. 左侧：实现 `On Stats Changed(DisplayName, Vitals, Stats)`。血蓝条可以用 `Vitals` 的 `GetHealthPercent` / `GetManaPercent`；属性列表遍历 `Stats`，显示 `Label` 和 `ValueText`。
3. 右侧二选一：
   - **自动生成**：放一个 VerticalBox 或 ScrollBox，命名为 `BuffList`，类默认值设置 `Buff Entry Class`。条目按 `InstanceId` 复用：叠层、刷新时长、抑制只原地更新，不重建控件。
   - **自己生成**：实现 `On Buffs Changed(Buffs)`，剩余时间用 `Get Buff Remaining Time`。
4. Buff 列表为空时也会触发 `On Buffs Changed`，可以在这里切换“当前没有效果”的提示。
5. 默认 `Input Config = Menu`、允许返回键关闭；关闭按钮调用 `Close Screen`。

**条目 `WBP_CharacterBuffEntry`**

1. 父类选 `CC_CharacterBuffEntryWidget`。
2. 按需放置并命名：`Icon`（Image）、`NameText`、`DescriptionText`、`TimeText`（TextBlock）、`TimeBar`（ProgressBar），都可以省略。
3. 实现 `On Buff Set`：按 `bIsDebuff` 切换配色，按 `bIsInhibited` 半透明显示。
4. 层数大于 1 时名称显示为“分裂箭 ×2”，格式由类默认值 `Name With Stacks Format` 决定；无限期文字默认为“持续”。

**打开面板**

在玩法输入（例如 C 键）里：

```text
Get Owning Player → Get Root Layout For Player
→ Show Screen
    Layer        = UI.Layer.GameMenu
    Screen Class = WBP_CharacterScreen
```

面板打开后是 Menu 输入，C 键不会再走到玩法输入。想让 C 键也能关闭面板，就在 WBP 里监听该键并调用 `Close Screen`。

## 当前未覆盖

- **等级**：项目里还没有角色等级的数据来源，等级系统落地后在 `RefreshStats` 中补上即可。
- **查看其他角色**：目前固定显示本地玩家当前的 Pawn。以后可以通过 ShowScreen 的 Context 传入目标角色，由 Controller 优先使用。

## 验证清单

本次只做了源码静态检查和 `Tools/HarnessChecks/check_buff_rules.sh`（0 错误），未编译、未运行 PIE。编译后建议验证：

- 打开面板：属性和 Buff 首次正确显示；关闭再打开不重复订阅。
- 受伤、攻击 Buff 生效时，左侧数值实时变化，Buff 列表不闪烁。
- 拾取、叠层、刷新时长、抑制和恢复、自然到期、被驱散，右侧列表都正确更新。
- `bHiddenInUI` 的 Buff、没有 UIData 的 GE 不出现。
- 面板打开期间死亡重生：换到新 Pawn 后数据正确，不残留旧角色数据。
- 联机客户端：倒计时与服务器一致；单机打开面板时倒计时冻结。
