# CombatMain 与角色信息界面：蓝图创建、布局和游戏接入

本说明按项目现有 C++ 接口编写，供后续直接修订。核对日期：2026-10-21。

目标：进入战斗地图显示 CombatMain；血蓝、波次、经验、击杀随数据变化更新；从 CombatMain 打开角色信息页面；左侧显示名字、血蓝、属性，右侧显示当前 Buff；关闭后恢复游戏操作。

角色页面使用已有的 **UCC_CharacterScreenWidget**，控制器和模型也已存在。页面不需要自行查询 ASC，不需要通过 Tick 轮询属性。

## 1. 现有实现与本教程新增内容

| 内容 | 当前来源 | 蓝图需要做什么 |
| --- | --- | --- |
| CombatMain 血蓝 | CC_CombatMainController → CC_CombatMainModel | 放入同名必需子控件，C++ 自动推送 |
| 波次、击杀、团队经验 | CombatMainModel.GetSurvival | 实现 On UI Model Changed，更新文字和进度条 |
| 角色名字、血蓝、属性 | CC_CharacterScreenController → CC_CharacterScreenModel | 实现 On Stats Changed |
| 当前 Buff | CharacterScreenController 读取 BuffComponent 与 GE 的 UIData | 配置自动生成容器、Buff Entry Class |
| Buff 倒计时 | CC_CharacterBuffEntryWidget | 放置约定名称的文字和进度条，原生代码刷新 |
| 技能栏 | 本教程提供布局示例 | 技能图标、快捷键外观；真实冷却和激活状态尚需接入 |
| 装备、换装、实时 3D 预览 | 不在现有角色页面实现范围内 | 如需这些功能，另行扩展数据和表现 |

页面层级：

```text
CC_RootLayout（框架创建）
├─ UI.Layer.Game
│  └─ WBP_CombatMain
└─ UI.Layer.GameMenu
   └─ WBP_CharacterScreen（打开时加入，关闭后退出）
```

相关说明：[UI 框架](UIFramework.zh-CN.md)、[生命周期与地图装配](LyraUI-Phase1.zh-CN.md)、[角色面板架构](CharacterScreen.zh-CN.md)、[控件绑定迁移](UIWidgetBindings.zh-CN.md)。

## 2. 名称、绑定和布局控件

新建布局控件采用 `类型_用途`，例如 `TextBlock_Wave`、`VerticalBox_Stats`。资产名称和控件实例名称不同：`WBP_PlayerVitals` 是资产；拖入 CombatMain 后实例必须命名为 `PlayerVitalsWidget_PlayerVitals`。

### 2.1 当前 C++ 要求的绑定名称

| 所在蓝图父类 | Designer 中的实例名称 | 类型 | 绑定 |
| --- | --- | --- | --- |
| CC_CombatMainWidget | PlayerVitalsWidget_PlayerVitals | CC_PlayerVitalsWidget 或派生蓝图 | 必需 |
| CC_PlayerVitalsWidget | ProgressBar_Health | ProgressBar | 必需 |
| CC_PlayerVitalsWidget | ProgressBar_Mana | ProgressBar | 必需 |
| CC_PlayerVitalsWidget | TextBlock_Vitals | TextBlock | 必需 |
| CC_CharacterScreenWidget | BuffList | PanelWidget 的派生类型，本教程用 ScrollBox | 可选；自动列表方案需提供 |
| CC_CharacterBuffEntryWidget | Icon | Image | 可选 |
| CC_CharacterBuffEntryWidget | NameText | TextBlock | 可选 |
| CC_CharacterBuffEntryWidget | DescriptionText | TextBlock | 可选 |
| CC_CharacterBuffEntryWidget | TimeText | TextBlock | 可选 |
| CC_CharacterBuffEntryWidget | TimeBar | ProgressBar | 可选 |

**当前角色类中的 BuffList、Icon、NameText 等仍是旧命名，尚未统一为类型前缀。为让本说明可以直接照做，保留源码中的实际名称。不要只在 Designer 中改为 ScrollBox_Buffs、Image_Icon 等，否则原生自动绑定失效。后续若统一源码命名，须同步修改本表及下文。**

必需项使用 BindWidget：缺少同名、兼容类型控件时，派生 Widget Blueprint 编译报错。可选项缺失不会报错；名字拼错可能导致运行时不显示。需要在 Graph 中操作的控件勾选 Is Variable。

### 2.2 “容器”具体是什么

| Palette 中的类型 | 用途 | 子控件数量 |
| --- | --- | --- |
| Canvas Panel | 用锚点、坐标安排屏幕各区域 | 多个 |
| Horizontal Box | 从左到右排列 | 多个 |
| Vertical Box | 从上到下排列 | 多个 |
| Overlay | 叠放，例如图标上覆盖快捷键 | 多个 |
| Size Box | 指定宽高或最小最大尺寸 | 一个 |
| Border | 背景、边框、内边距 | 一个 |
| Scroll Box | 可滚动列表 | 多个 |

Slot 由父控件决定：Canvas 的直接子项才有 Canvas Panel Slot 的 Anchors、Position；Vertical Box 下用 Vertical Box Slot 的 Size、Padding、Alignment。

## 3. 创建蓝图资产

Content Browser 中创建 Widget Blueprint，父类选择窗口展开 All Classes 搜索对应类。如果已有派生蓝图，直接修改现有资产，避免重复建页面。

| 推荐目录（相对 Content） | 资产 | 父类 |
| --- | --- | --- |
| GAS_Demo/UI/Combat | WBP_PlayerVitals | CC_PlayerVitalsWidget |
| GAS_Demo/UI/Combat | WBP_SkillSlot | UserWidget |
| GAS_Demo/UI/Combat | WBP_CombatMain | CC_CombatMainWidget |
| GAS_Demo/UI/Character | WBP_CharacterStatRow | UserWidget |
| GAS_Demo/UI/Character | WBP_CharacterBuffEntry | CC_CharacterBuffEntryWidget |
| GAS_Demo/UI/Character | WBP_CharacterScreen | CC_CharacterScreenWidget |

推荐先完成血蓝子控件、属性行、Buff 条目，再搭两个主页面。C++ 反射类发生修改后，应完成 C++ 编译并重新打开编辑器；本次仅添加说明文档，不会创建上述资产。

## 4. 血蓝条 WBP_PlayerVitals

### 4.1 Designer 结构

```text
SizeBox_Root                         [Size Box]
└─ Border_Background                 [Border]
   └─ VerticalBox_Content            [Vertical Box]
      ├─ TextBlock_Vitals            [Text Block，必需]
      ├─ SizeBox_Health              [Size Box]
      │  └─ ProgressBar_Health       [Progress Bar，必需]
      └─ SizeBox_Mana                [Size Box]
         └─ ProgressBar_Mana         [Progress Bar，必需]
```

1. 将 Size Box 拖为根节点，命名 SizeBox_Root；设置 Width Override = 320。
2. 依次加入 Border 和 Vertical Box；Border 设置深色半透明背景，Padding = 12。
3. 向 Vertical Box 添加 Text Block 和两个 Size Box，按图命名。
4. 两个 Size Box 内分别放 Progress Bar，必须使用表中的名称。
5. SizeBox_Health 的 Height Override = 20；SizeBox_Mana = 12。
6. TextBlock_Vitals 字号先用 16，Vertical Box Slot 的 Padding Bottom = 8。
7. SizeBox_Health 的 Vertical Box Slot 的 Padding Bottom = 6。
8. 生命条 Fill Color 设为红色或绿色，法力条设为蓝色。编译并保存。

可在设计时将 Percent 设为 0.7 预览；运行时 C++ 会覆盖为真实比例。不添加进度条 Binding，也不从子控件查询玩家。父页面调用 SetVitals 后，C++ 更新文字和两个进度条。

## 5. 技能格 WBP_SkillSlot（外观示例）

```text
SizeBox_Root                         [72 × 72]
└─ Overlay_Content
   ├─ Border_Background
   ├─ Image_Icon
   ├─ Image_Cooldown
   └─ TextBlock_Key
```

1. Size Box 的 Width/Height Override 都设为 72。
2. Border 设深色底；Image_Icon 的 Overlay Slot 设置水平、垂直 Fill，Padding = 8。
3. Image_Cooldown 先设 Collapsed，留作后续冷却显示。
4. TextBlock_Key 的 Overlay Slot 水平靠右、垂直靠下，Padding = 4。
5. 创建 SkillIcon（Texture2D Object Reference）和 KeyLabel（Text）两个变量，勾选 Instance Editable。
6. 在 Event Pre Construct 中：Image_Icon.SetBrushFromTexture(SkillIcon)，TextBlock_Key.SetText(KeyLabel)。

这一步不实现技能释放与冷却。界面显示 Q/E/R/F 不会自动创建对应输入；KeyLabel 应填写项目真实按键。

## 6. 战斗主界面 WBP_CombatMain

### 6.1 Designer 完整结构

根节点使用 Canvas Panel。将已创建的用户控件从 Palette 的 User Created 分类或 Content Browser 拖入布局。

```text
CanvasPanel_Root
├─ PlayerVitalsWidget_PlayerVitals     [WBP_PlayerVitals，必需绑定]
├─ VerticalBox_WaveInfo
│  ├─ TextBlock_Wave
│  └─ TextBlock_AliveEnemies
├─ VerticalBox_KillInfo
│  ├─ TextBlock_MyKills
│  └─ TextBlock_TeamKills
├─ VerticalBox_Bottom
│  ├─ HorizontalBox_Skills
│  │  ├─ SkillSlotWidget_Skill1         [WBP_SkillSlot]
│  │  ├─ SkillSlotWidget_Skill2
│  │  ├─ SkillSlotWidget_Skill3
│  │  └─ SkillSlotWidget_Skill4
│  └─ VerticalBox_Progression
│     ├─ TextBlock_TeamLevel
│     └─ SizeBox_Experience
│        └─ ProgressBar_Experience
└─ Button_Character                   [普通 UMG Button]
   └─ TextBlock_Character
```

除必需血蓝子控件外，这些都是建议的蓝图布局变量；仅命名不会自动绑定波次、击杀或按钮行为。

### 6.2 屏幕位置和内部排列

选中根 Canvas，将 Visibility 设为 **Not Hit-Testable (Self Only)**。纯展示子区域可以设为 Not Hit-Testable (Self & All Children)，但不要将有按钮的整棵树一起禁用命中测试。

根 Canvas 的直接子项设置如下。固定锚点的 Min/Max 相同，位置相对该锚点。

| 控件 | Anchors | Alignment | Position | 尺寸 |
| --- | --- | --- | --- | --- |
| PlayerVitalsWidget_PlayerVitals | 左上 (0,0) | (0,0) | (32,32) | Auto Size |
| VerticalBox_WaveInfo | 顶中 (0.5,0) | (0.5,0) | (0,32) | Auto Size |
| VerticalBox_KillInfo | 右上 (1,0) | (1,0) | (-32,32) | Auto Size |
| VerticalBox_Bottom | 底中 (0.5,1) | (0.5,1) | (0,-32) | Auto Size |
| Button_Character | 右下 (1,1) | (1,1) | (-32,-32) | 120 × 44 |

Auto Size 在编辑器版本中也可能显示为 Size To Content。根 Canvas 自身随页面占满屏幕，不用设置子项那样的锚点。

- 波次区两个 TextBlock 的 Vertical Box Slot 水平居中，字号建议 28/18，示例文字“第 1 波”“剩余敌人：20”。
- 击杀区两个 TextBlock 水平靠右，示例文字“我的击杀：0”“团队击杀：0”。
- HorizontalBox_Skills 在 Vertical Box Slot 中水平居中，各技能格 Horizontal Box Slot 左右 Padding = 4。
- 给四个技能格设置各自 SkillIcon 和 KeyLabel。
- SizeBox_Experience 的 Width Override = 360、Height Override = 8；等级文字居中，技能栏与经验区域之间留 8 像素 Padding。
- TextBlock_Character 填“角色”。Button_Character 使用普通 Button，因此前缀是 Button；如改用 CommonButtonBase 派生蓝图，按真实类型重新命名和接事件。

Class Defaults 保留继承配置：Controller Class = CC_CombatMainController、Input Config = Game、Allow Back/Allow Close = false。编译并保存，确认必需绑定没有错误。

### 6.3 波次、经验和击杀接线

Graph 添加 On UI Model Changed 事件，输入是 UCC_UIModel：

```text
Event On UI Model Changed(Model)
    → Cast To CC_CombatMainModel(Model)
    → Get Survival
    → Break CC_SurvivalHUDSnapshot
```

用 Format Text 生成文字，再调用对应 TextBlock 的 Set Text：

| 快照字段 | 控件 | Format Text 示例 |
| --- | --- | --- |
| CurrentWave | TextBlock_Wave | 第 {Wave} 波 |
| AliveEnemies | TextBlock_AliveEnemies | 剩余敌人：{Count} |
| MyKills | TextBlock_MyKills | 我的击杀：{Count} |
| TeamKills | TextBlock_TeamKills | 团队击杀：{Count} |
| TeamLevel | TextBlock_TeamLevel | 团队等级 {Level} |

经验比例必须防止除零，用 Branch 分支计算：

```text
ExperienceToNextLevel > 0
    true  → TeamExperience / ExperienceToNextLevel → Clamp(0,1) → Set Percent
    false → ProgressBar_Experience.SetPercent(0)
```

根据 bActive 设置 VerticalBox_WaveInfo、VerticalBox_KillInfo、VerticalBox_Progression 的 Visibility：true 为 Not Hit-Testable (Self & All Children)，false 为 Collapsed。不要隐藏包含技能栏的整个 VerticalBox_Bottom。

此方案不需要调用 GetLevelProgress：该结构体的 C++ 辅助方法并未作为蓝图 UFUNCTION 暴露。血蓝已由 C++ 分发给 PlayerVitalsWidget_PlayerVitals，无需重复刷新。

## 7. 角色属性行 WBP_CharacterStatRow

父类 UserWidget，根节点使用 Horizontal Box：

```text
HorizontalBox_Root
├─ TextBlock_Label                 [Slot Size = Fill]
└─ TextBlock_Value                 [Slot Size = Auto]
```

两个 TextBlock 勾选 Is Variable，字号建议 20；可用外层 Border 增加每行 6～8 像素 Padding。

**在 `WBP_CharacterStatRow` 的函数列表中新建 `SetStat`**，添加输入 `Stat`，类型选择 `FCC_CharacterStatEntry`（蓝图类型选择器中可能显示去掉 F 前缀后的名称）。`UserWidget` 没有自带 `SetStat`，项目 C++ 也没有为属性行提供同名函数；必须先创建此函数并编译保存条目蓝图，第 9.3 节才能从 `Create Widget` 的返回值调用它。函数实现：

```text
SetStat(Stat)
    → Break CC_CharacterStatEntry
        Label     → TextBlock_Label.SetText
        ValueText → TextBlock_Value.SetText
```

`TextBlock_Label`、`TextBlock_Value` 都要勾选 **Is Variable**，在 `SetStat` 函数图表里从变量拖出并调用 `Set Text`。`ValueText` 已经格式化，不要把百分比再乘 100，也不要按显示标签字符串判断这是哪种属性。

## 8. Buff 条目 WBP_CharacterBuffEntry

父类选择 **CC_CharacterBuffEntryWidget**。以下绑定名称沿用当前源码：

```text
Border_Background
└─ HorizontalBox_Content
   ├─ SizeBox_Icon                  [48 × 48]
   │  └─ Icon                      [Image]
   └─ VerticalBox_Details           [Horizontal Box Slot Size = Fill]
      ├─ NameText                   [TextBlock]
      ├─ DescriptionText            [TextBlock]
      ├─ TimeText                   [TextBlock]
      └─ SizeBox_TimeBar            [Height Override = 6]
         └─ TimeBar                [ProgressBar]
```

1. Border Padding = 12，使用深色背景。
2. VerticalBox_Details 左侧 Padding = 12；文字间留 4 像素。
3. DescriptionText 开启 Auto Wrap Text；长描述可给右侧详情区增加合理的宽度约束。
4. 编译保存。图标、名称、描述、叠层名称、倒计时和进度由原生代码更新，无需自写 Tick。
5. 可选实现 On Buff Set：bIsInhibited 为 true 时 Set Render Opacity(0.4)，否则必须恢复为 1；bIsDebuff 用于背景配色。

类默认值 Name With Stacks Format 决定叠层名称，默认“{Name} ×{Stacks}”；Infinite Duration Text 默认为“持续”。没有图标数据时原生代码隐藏 Icon；无限期效果的 TimeBar 为满。

这些绑定目前可选，省略某项只是不显示该项。本教程为完整展示放齐五项。不要再添加设计器属性 Binding 与原生 SetText/SetPercent 竞争。

## 9. 角色页面 WBP_CharacterScreen

本节的页面 Designer、Class Defaults 和事件接线（9.1～9.5）**都在 `WBP_CharacterScreen` 中操作**；它是继承 `CC_CharacterScreenWidget` 的页面蓝图，不是属性行或 Buff 条目蓝图。涉及其他蓝图时按下表切换：

| 蓝图 | 本教程在其中完成的工作 |
| --- | --- |
| `WBP_CharacterScreen` | 9.1～9.2 搭页面；9.3 在 Event Graph 响应属性变化并创建属性行；9.4 响应 Buff 列表变化；9.5 接关闭按钮 |
| `WBP_CharacterStatRow` | 第 7 节搭一行属性，并在自己的函数图表实现 `SetStat(Stat)`；9.3 只是调用它 |
| `WBP_PlayerVitals` | 第 4 节搭血蓝子控件；9.3 中由页面调用它的 C++ `SetVitals` |
| `WBP_CharacterBuffEntry` | 第 8 节搭 Buff 条目；9.4 的条目由页面的 C++ 自动创建，不在页面事件中手动创建 |
| `BP_CharacterScreenController`（可选） | 只在 9.6 修改属性显示配置；这是普通 Blueprint Class，不是 Widget Blueprint |

### 9.1 父类和 Designer 结构

打开 **`WBP_CharacterScreen` 的 Designer**，按下图搭页面。父类必须是 **CC_CharacterScreenWidget**，不是普通 CC_ActivatableWidget，也不是 CombatMain。属性和 Buff 事件由这个类提供。

```text
Overlay_Root
├─ Border_Shade
└─ SizeBox_Window
   └─ Border_Window
      └─ VerticalBox_Content
         ├─ HorizontalBox_Header
         │  ├─ TextBlock_Title
         │  └─ Button_Close
         │     └─ TextBlock_Close
         └─ HorizontalBox_Body
            ├─ VerticalBox_Stats
            │  ├─ TextBlock_PlayerName
            │  ├─ PlayerVitalsWidget_Vitals       [WBP_PlayerVitals]
            │  └─ ScrollBox_Stats
            │     └─ VerticalBox_StatList
            └─ VerticalBox_Effects
               ├─ TextBlock_BuffTitle
               ├─ TextBlock_NoBuffs
               └─ BuffList                       [ScrollBox，绑定名]
```

这里的 PlayerVitalsWidget_Vitals 是角色页自己的血蓝子控件，可自由命名，由事件显式调用 SetVitals；它不受 CombatMain 的必需名称约束。

### 9.2 大小、对齐和焦点

继续在 **`WBP_CharacterScreen` 的 Designer** 中选中对应控件，修改下表所列的 Slot 和外观；末尾的 Class Defaults 也是这个页面蓝图的默认值。

| 控件 | 设置 |
| --- | --- |
| Border_Shade | Overlay Slot 水平/垂直 Fill；黑色，Alpha = 0.6 |
| SizeBox_Window | Overlay Slot 水平/垂直 Center；Width = 1000，Height = 650 |
| Border_Window | 深色底，Padding = 24 |
| TextBlock_Title | “角色信息”，字号 28；Horizontal Box Slot Size = Fill |
| Button_Close | Slot Size = Auto，Is Focusable = true |
| TextBlock_Close | “关闭” |
| HorizontalBox_Body | Vertical Box Slot Size = Fill；Padding Top = 24 |
| VerticalBox_Stats | Horizontal Box Slot Size = Fill（权重 1），Padding Right = 16 |
| VerticalBox_Effects | Horizontal Box Slot Size = Fill（权重 1），Padding Left = 16 |
| ScrollBox_Stats | Vertical Box Slot Size = Fill |
| BuffList | Vertical Box Slot Size = Fill |
| TextBlock_BuffTitle | “当前效果”，字号 24 |
| TextBlock_NoBuffs | “当前没有效果” |

1000 × 650 是桌面布局起点，不保证小分辨率可容纳；需要小窗口支持时，可在窗口外增加 ScaleBox（Scale To Fit）后重新检查文字可读性。在 Designer 的不同 Screen Size 下检查。

Class Defaults：

```text
Controller Class              = CC_CharacterScreenController（继承默认值）
Input Config                  = Menu
Allow Back                    = true
Allow Close                   = true
Default Focus Widget Name     = Button_Close
Buff Entry Class              = WBP_CharacterBuffEntry
```

不要把覆盖整个页面的节点设为 Not Hit-Testable (Self & All Children)，否则关闭按钮不能点击。

### 9.3 On Stats Changed 接线

先完成第 7 节，并编译保存 **`WBP_CharacterStatRow`**，确认它有自建函数 `SetStat(Stat)`。然后回到 **`WBP_CharacterScreen` 的 Event Graph**，在 Overrides/事件搜索中添加 `On Stats Changed`，得到 `DisplayName`、`Vitals`、`Stats` 三个输出：

```text
Event On Stats Changed(DisplayName, Vitals, Stats)
    → TextBlock_PlayerName.SetText(DisplayName)
    → PlayerVitalsWidget_Vitals.SetVitals(Vitals)
    → VerticalBox_StatList.ClearChildren
    → For Each Loop(Stats)
        → Create Widget(WBP_CharacterStatRow)
            Owning Player = Get Owning Player
        → 返回的行控件.SetStat(Array Element)
        → VerticalBox_StatList.AddChildToVerticalBox(该行控件)
```

这里的 `SetStat` 调用节点来自刚才创建的 **`WBP_CharacterStatRow` 函数**，不是 `UserWidget` 的内置节点。`Create Widget` 的 Class 直接选 `WBP_CharacterStatRow`，从它的返回值拖出 `SetStat`；如果返回引脚被当作通用 `UserWidget`，先检查 Class 是否设为具体蓝图类型。`Array Element` 接 `SetStat` 的 `Stat` 输入。

属性数量较少，这个版本在属性快照变化时重建属性行，接线简单。若以后更新频繁，可缓存行控件按属性更新；不要在 Buff 事件中重复重建属性区。

血蓝复用 WBP_PlayerVitals，避免自行计算。若使用自定义进度条，在蓝图 Break Vitals 后防止 MaxHealth/MaxMana 为零再计算并 Clamp。不要照抄调用 GetHealthPercent/GetManaPercent：当前它们只是结构体的 C++ 辅助方法，不是可直接搜索到的蓝图节点。

### 9.4 Buff 自动生成与空状态

本段在 **`WBP_CharacterScreen`** 中操作：Designer 放容器、Class Defaults 指定条目类、Event Graph 只切换空状态。本教程选择原生自动生成方案。必须同时满足：

1. Designer 中存在 **BuffList**，类型为 ScrollBox 或其他 PanelWidget 派生类。
2. Class Defaults 的 **Buff Entry Class** 已选 WBP_CharacterBuffEntry。

不要在 BuffList 内预先放静态标题或空状态文字；原生逻辑管理并清理它的子项。标题和 TextBlock_NoBuffs 按上图放在它外面。

```text
Event On Buffs Changed(Buffs)
    → Length(Buffs) == 0
        true  → TextBlock_NoBuffs.SetVisibility(Visible)
        false → TextBlock_NoBuffs.SetVisibility(Collapsed)
```

C++ 在触发 On Buffs Changed 前已经同步了条目。这个事件只处理空状态和额外表现，不再 ClearChildren 或自行 CreateWidget，避免破坏原生条目复用。

数据路径为 BuffComponent.GetActiveBuffs + GE 的 CC_BuffUIData；没有 UIData 或勾选 bHiddenInUI 的效果不会显示。叠层、刷新时长、抑制会更新已有条目，增删时才调整列表。剩余时间由条目按服务器时间计算。

### 9.5 关闭页面

在 **`WBP_CharacterScreen` 的 Event Graph** 中，选 Designer 里的 `Button_Close` 添加 `On Clicked`：

```text
Button_Close.OnClicked
    → Close Screen（Target = Self）
```

Esc/手柄返回使用现有 CommonUI 配置。不要用 RemoveFromParent、直接 DeactivateWidget 或 AddToViewport 代替项目的页面栈接口。

### 9.6 调整属性列表

需要更改显示顺序、标题或格式时，**离开页面蓝图**，创建普通 Blueprint Class，父类选 CC_CharacterScreenController，例如 BP_CharacterScreenController。这是 UObject 控制器蓝图，不是 Widget Blueprint。

1. Class Defaults 中修改 Displayed Stats，每项指定 Attribute、Label、Format。
2. WBP_CharacterScreen 的 Controller Class 选这个派生控制器。
3. 默认配置是攻击力/防御（Integer）、暴击率/暴击伤害/移动速度倍率（Percent）。ValueText 已格式化。
4. Debuff Tag 留空时全部按增益处理；需要分类时选择项目真实使用的标签，不凭空添加具体 Buff 判断。

角色页显示的是本地当前 Pawn。查看其他角色、独立角色等级、装备和换装不属于当前接口提供的功能；团队等级也不应直接当作角色个人等级。

## 10. 从 CombatMain 打开角色页

在 WBP_CombatMain 中选 Button_Character，添加 OnClicked：

```text
Button_Character.OnClicked
    → Get Owning Player
    → Get Root Layout For Player（Player = Owning Player，返回 Root）
    → Is Valid（Root）的有效执行引脚
    → 从 Root 引脚拖出 Has Menu、Is Input Blocked
    → NOT(Has Menu) 与 NOT(Is Input Blocked) → AND → Branch
    → Branch True：从 Root 引脚拖出 Show Screen
        Layer        = UI.Layer.GameMenu
        Screen Class = WBP_CharacterScreen
        Context      = 空
```

`Has Menu` 和 `Is Input Blocked` 是 **`CC_RootLayout` 的 BlueprintPure 成员函数**，不是空白处随处可用的全局节点；必须先从 `Get Root Layout For Player` 得到类型为 `CC_RootLayout` 的 `Root`，再从该引脚拖线搜索。这个 Branch 只是本页面避免菜单已打开或输入被阻断时再次请求的门控，不是 `Show Screen` 的必需参数。

检查 `Show Screen` 返回页面是否有效；失败时先检查 Root、Layer、页面类与过渡状态。ShowScreen 在过渡或同步重入期间会返回空，不会自动排队。若省略上述 `Has Menu` 门控，同层同类页面可返回已有实例，不会重复叠加。

角色页不需要持有 CombatMain 引用，CombatMain 也不必缓存角色页实例。数据订阅由每个页面自己的 Controller 管理。

HUD 默认 Game 输入。如果当前玩法捕获鼠标、隐藏光标，单靠布局不能使按钮可点击；需要沿用项目的光标玩法或添加明确的菜单访问方式。可在 PlayerController 的现有 Enhanced Input Mapping Context 中添加角色菜单 Input Action（例如 C），触发相同的导航逻辑。不要为了一个按钮把整个 HUD 长期改成 Menu 输入。

Menu 打开后玩法 C 键通常不会继续触发；先用关闭按钮和 Esc 返回。若还要 C 关闭，需要在 UI 输入侧另外配置。

RootLayout 默认在单机有菜单时暂停世界，联机不因此暂停服务器。实时查看需求可在 BP_RootLayout 中关闭 Pause Game While Menu Open；这是根布局层面的开关，影响其他菜单。暂停与输入模式是两件事。

## 11. 战斗地图实装：初始显示 CombatMain

采用项目文档推荐的地图装配入口。可以使用战斗地图 Level Blueprint，或仅放在战斗地图里的独立 UI 装配 Actor。下面按一个本地玩家说明；不要在前端地图无条件显示战斗 HUD。

### 11.1 创建装配事件

创建自定义事件 TryShowCombatHUD，输入 Player（PlayerController）。执行：

```text
TryShowCombatHUD(Player)
    → Is Valid(Player)
    → Is Local Controller
    → 确认 Player.GetWorld 对应本装配入口所在的战斗世界
    → Get Root Layout For Player(Player)
    → Is Valid(Root)
    → Root.Show Screen
        Layer        = UI.Layer.Game
        Screen Class = WBP_CombatMain
        Context      = 空
```

只在返回有效页面后记录装配成功。若采用 bHUDInstalled 标记，在同一 Actor/世界实例内防止重复，并在 Root 重新挂载、原页面被清理时允许重新装配；不要将此标记跨地图永久保存在 GameInstance。也可直接依赖同层同类页面去重。

### 11.2 BeginPlay、通知和 EndPlay

```text
Event BeginPlay
    → Get Game Instance Subsystem（CC_UIManagerSubsystem）
    → 保存 UIManager 引用
    → Bind Event to OnRootLayoutReady（绑定自己的 HandleRootReady 事件）
    → Get Player Controller(0)
    → TryShowCombatHUD(该 Player)

HandleRootReady(Player, Root)
    → 校验本地玩家与当前战斗世界
    → TryShowCombatHUD(Player)

Event EndPlay
    → 若 UIManager 有效
    → Unbind Event from OnRootLayoutReady（解除同一个 HandleRootReady 事件）
```

先订阅、再立即查询，覆盖 Root 已经存在和稍后准备好两种情况。不要使用 Unbind All Events，以免移除其他系统的订阅。切图页面清理由框架处理，无需自行销毁 Root。

检查初始装配的 ShowScreen 返回值。若被正在播放的栈过渡拒绝，可使用 Show Screen Async 请求，传相同 Root/Layer、页面软类引用，并将 HUD 请求的 Suspend Input 设为 false；它会等待可入栈时机。保存请求以便装配 Actor EndPlay 时取消，避免在旧地图结束后继续装配。Root 尚未出现时仍应等待 OnRootLayoutReady，异步显示不能代替 Root 的创建。

不要再额外 CreateWidget(WBP_CombatMain).AddToViewport，也不要手工创建 Root；这样会绕过框架的模型和页面生命周期管理。

### 11.3 Policy 检查

现有 DefaultGame.ini 配置 DefaultUIPolicyClass 指向 /Game/GAS_Demo/UI/Framework/BP_UIPolicy。需要自定义根布局时，在该 Policy 的 Default Root Layout Class 选择 WBP_RootLayout。

根布局原生构建页面栈，不需要在 WBP_RootLayout Designer 中再拖四个栈。Layers 留空使用内置层；如果自定义了 Layers，必须包含本教程使用的 UI.Layer.Game 和 UI.Layer.GameMenu，Kind 分别为 Game 和 Menu。

## 12. 验证与排错

依次编译保存六个 Widget Blueprint。C++ 编译成功不代表 Designer 控件名称和蓝图接线已正确，也不代表以下操作已在游戏里验收。

| 检查 | 预期 |
| --- | --- |
| CombatMain 蓝图编译 | PlayerVitalsWidget_PlayerVitals 不缺失且类型正确 |
| PlayerVitals 蓝图编译 | 三个必需绑定全部正确 |
| 进入战斗地图 | 一份 HUD，无重复 AddToViewport |
| 受伤、回蓝、改变最大值 | HUD 血蓝自动更新 |
| 普通地图/生存地图 | 普通地图隐藏生存区；生存地图显示真实波次、经验、击杀 |
| 角色按钮/快捷键 | GameMenu 层打开角色页，输入转为菜单 |
| 角色页首次打开 | 名字、血蓝、默认五项属性正确显示 |
| 没有 Buff | 显示空状态，不残留旧条目 |
| Buff 新增、叠层、刷新、抑制、移除 | 条目正确更新，叠层和刷新不重建整个列表 |
| 单机菜单暂停 | Buff 倒计时随世界暂停；关闭后恢复 |
| 联机客户端 | 列表随复制更新，倒计时按服务器时钟计算 |
| 关闭并重开/换 Pawn/重生 | 新数据正确，无重复订阅、旧角色数据残留 |
| 多种分辨率 | 区域保持锚点，窗口不被裁切，描述可滚动 |
| 切到前端、再进入战斗 | 旧页面清理，新地图重新装配 |

常见问题：

- 只有背景没有血蓝：检查主页面的子控件实例名，及 WBP_PlayerVitals 内部三个名称和类型。
- 波次都是 0：先检查 GetSurvival.bActive 和地图 GameState 是否为生存模式，再检查 On UI Model Changed 接线。
- 角色页面没有 On Stats Changed：检查父类是否为 CC_CharacterScreenWidget。
- Buff 列表不出现：检查 BuffList 名称、Buff Entry Class、GE 的 UIData 和 bHiddenInUI。
- Buff 条目有空白文字：当前绑定可选，先检查 NameText、DescriptionText 等是否与源码完全一致。
- 属性行重复：检查是否在 ForEach 前清空 VerticalBox_StatList。
- 关闭按钮不能点击：检查祖先 Visibility，避免禁用整棵树的命中测试；确认没有过渡输入遮罩仍在工作。
- Esc 停止 PIE：使用独立游戏窗口验证完整返回流程。

## 13. 后续修订位置

- 角色控件命名若统一为类型前缀：同步修改第 2、8、9 节及 CharacterScreen.zh-CN.md。
- 真实技能冷却/可用状态接入后：补全第 5、6 节，不能只删除“外观示例”而缺少数据接线。
- 装备、角色等级或查看其他角色实现后：更新第 1、9 节，并明确真实数据来源。
- 验收结果记录实际蓝图和地图名称，不将本教程中的建议资产名当作已创建资产。
