# Buff 三选一

肉鸽式的 Buff 选择：服务器从 Buff 池里随机抽几个选项（默认 3 个），玩家选一个，服务器施加对应的 GE。联机时每个玩家各自抽、各自选。

本系统只负责 Buff 的"获取"（见 [BuffSystem.zh-CN.md](BuffSystem.zh-CN.md) 第 3 节）。选中之后就是一次普通的 `UCC_BuffComponent::ApplyBuff`，叠层、免疫、持续时间都由 GE 自己决定。

## 类一览

| 类 | 位置 | 职责 |
|---|---|---|
| `UCC_BuffOfferPool` | `Data/Buff/` | 数据资产：候选 Buff 列表 + 每轮数量；运行时只读 |
| `UCC_BuffSelectionComponent` | `Components/` | 挂在 PlayerController 上（已在 `ACC_PlayerController` 构造函数里创建）。服务器抽取、排队、校验、施加；客户端发请求、自动打开界面 |
| `UCC_BuffSelectionScreen` | `UI/BuffSelection/` | 页面基类（`CC_ActivatableWidget`），自动生成卡片，选完自动关闭 |
| `UCC_BuffSelectionController` / `Model` | `UI/BuffSelection/` | 监听选择组件，把当前一轮转成卡片快照；转发点击 |
| `UCC_BuffOfferCardWidget` | `UI/BuffSelection/` | 单张卡片，继承 `CommonButtonBase`，点击即选择 |

## 流程

```text
服务器  OfferBuffs(Pool)
          ├─ Pool.RollOptions：过滤无效条目 → 不放回随机抽 OfferCount 个（同一 GE 一轮只出现一次）
          └─ 分配 OfferId；已有一轮未选完 → 排队，否则设为当前一轮
               ↓ 复制（仅拥有者）
客户端  OnRep → OnOfferChanged → 自动 ShowScreen(SelectionScreenClass, GameMenu 层)
          页面 Controller 读当前一轮 → Model → 卡片
          点击卡片 → RequestChoose(OfferId, Index) → ServerChoose RPC
               ↓
服务器  校验 OfferId 与下标 → 当前 Pawn 的 BuffComponent.ApplyBuff
          ├─ 成功：进入下一轮（或清空）
          └─ 失败（NoTarget / ApplyFailed）：本轮保留，玩家可改选
          ClientChoiceResolved → 界面显示失败原因或换成下一轮；没有下一轮时页面自动关闭
```

单机和监听服务器主机走同一套代码，RPC 在本地同步执行。

## 现有资产和分工

项目里已有 `/Game/GAS_Demo/GameMode/SurvivalGameMode/DA_BuffOfferPool` 和 `DA_SurvivalModeConfig`，但还没有三选一的页面、卡片 Widget Blueprint。按下面步骤新建两个 `WBP_`，不需要再写一套抽卡、RPC 或自动弹窗蓝图。每段标题都写明应打开的资产；不要把卡片事件接在页面蓝图上。

| 打开的资产 | 需要做什么 | C++ 已经做了什么 |
|---|---|---|
| `DA_BuffOfferPool`、各 Buff GE | 配置可抽选项及显示数据 | 抽取、过滤、去重 |
| `WBP_BuffOfferCard`（新建） | 单张卡片的布局、可选的额外视觉效果 | 填名称/说明/图标、处理点击 |
| `WBP_BuffSelection`（新建） | 页面布局、指定卡片类、可选状态提示 | 生成卡片、禁止重复提交、自动关闭 |
| `BP_CC_PlayerController` | 给 `BuffSelection` 组件指定页面类 | 收到选项后自动在 UI 层打开页面 |
| `DA_SurvivalModeConfig` | 升级时要自动三选一，就配置 `Progression.BuffPool` | 升级后向每个玩家发放选项 |

### 1. 配置 `DA_BuffOfferPool` 和 Buff GE

1. 打开已有的 `DA_BuffOfferPool`；`Entries` 中每项填写一个 Buff 的 Gameplay Effect 类和正数 `Level`。`Offer Count` 设为 3。有效条目少于 3 个时，页面只能显示实际抽到的数量。
2. 逐个打开所填的 GE，在 GE 的组件列表中加入 `CC Buff UI Data`，填写 `Display Name`、`Description` 和 `Icon`。不要把这些文字填在三选一页面上；卡片会从 GE 的 UI Data 读取。
3. 保存、验证数据资产。空 GE、抽象 GE、非法等级、缺少 UI Data 的条目会被池过滤；同一 GE 类在一轮里不会重复出现。若没有有效条目，`Offer Buffs` 返回 false，页面也不会弹出。

### 2. 新建 `WBP_BuffOfferCard`：单张卡片

在 `/Game/GAS_Demo/UI/BuffSelection/` 新建 Widget Blueprint，**父类选 `CC_BuffOfferCardWidget`**。它本身继承 `CommonButtonBase`，是可点击的卡片；不要再套一个普通 Button 并重复写选择请求。

**在 `WBP_BuffOfferCard` 的 Designer 中**安排紧凑的卡片内容，例如：

```text
Border_Card
└─ VerticalBox_Content
   ├─ Image_Icon               [Image，必需]
   ├─ TextBlock_Name           [TextBlock，必需]
   └─ TextBlock_Description    [TextBlock，必需]
```

三个带类型说明的控件必须**名称和类型都匹配**，并勾选 `Is Variable`；它们是 C++ `BindWidget` 字段，缺少任何一个都会导致该蓝图编译失败。卡片尺寸可先用约 220 × 300 的期望尺寸，图标用 64 × 64 的 SizeBox 约束，文字允许换行。颜色、边框和焦点样式在这张卡片的设计器/CommonButton 样式中设置。

**在 `WBP_BuffOfferCard` 的 Event Graph 中**，最小方案无需接线：C++ `SetCardData` 已写入名称、说明和图标，`NativeOnClicked` 已把本卡的 `OptionIndex` 交给所属页面。只有要显示额外的等级或稀有度时才添加 `Event On Card Data Set`，从 `CardData` 读取 `Level` 等字段；不要在此再调用 `Choose Option`，否则一次点击会提交两次。

编译并保存 `WBP_BuffOfferCard`，再做页面，否则页面的 `Card Class` 列表可能找不到它。

### 3. 新建 `WBP_BuffSelection`：三选一页面

在同一目录新建 Widget Blueprint，**父类选 `CC_BuffSelectionScreen`**，不是普通 `UserWidget` 或 `CC_ActivatableWidget`。**在 `WBP_BuffSelection` 的 Designer 中**放置全屏根布局和卡片面板：

```text
CanvasPanel_Root
├─ Border_Backdrop             [全屏暗色背景]
└─ VerticalBox_Center          [居中，尺寸由内容决定]
   ├─ TextBlock_Title          [例如“选择一项强化”]
   ├─ PanelWidget_Cards        [HorizontalBox，必需用于自动生成]
   └─ TextBlock_Result         [可选：失败提示/提交状态]
```

将 HorizontalBox **准确命名为 `PanelWidget_Cards`** 并勾选 `Is Variable`。它是可选绑定字段，但最小自动生成方案必须放它；不放时 C++ 不会创建任何可见卡片。给面板的子项设置间距、居中对齐，保证 1～3 张卡片也能正常排列。背景要挡住下层鼠标点击，卡片区不可设置为 `Not Hit-Testable (Self & All Children)`。

**在 `WBP_BuffSelection` 的 Class Defaults 中**设置 `Card Class = WBP_BuffOfferCard`。保留父类的 `Controller Class = CC_BuffSelectionController`、`Input Config = Menu` 和 `Allow Back = false`；误清空 `Controller Class` 会使模型为空。编译并保存。

**在 `WBP_BuffSelection` 的 Event Graph 中**，最小方案也无需接线。若放了 `TextBlock_Result`，可添加 `Event On Buff Offer Changed(Cards, bSubmitting, LastResult)`：`bSubmitting` 为 true 时显示“提交中”；`LastResult = NoTarget` 时提示需要存活角色；`ApplyFailed` 时提示当前 Buff 无法施加；`None` 时清空提示。这个事件只用于展示，不要在里面重新创建卡片或再次调用 `Choose Option`，因为父类已负责这些操作。页面的 `On UI Model Changed` 也不是必接事件。

### 4. 在 `BP_CC_PlayerController` 指定页面

打开 `/Game/GAS_Demo/Player/BP_CC_PlayerController`，在 Components 面板选中继承的 **`BuffSelection` 组件**，在 Details 的 `Buff | Selection | UI` 下设置：

```text
Selection Screen Class = WBP_BuffSelection
Screen Layer = UI.Layer.GameMenu
```

保存蓝图，并确认当前地图 GameMode 实际使用的是这个 PlayerController 类。`Selection Screen Class` 为空时，服务器仍可能生成选项，但本地不会自动弹窗。这里不需要在 `WBP_CombatMain` 里调用 `Create Widget`、`Add to Viewport` 或 `Show Screen`。

### 5. 选择触发来源

**生存模式升级自动触发：** 打开已有的 `/Game/GAS_Demo/GameMode/SurvivalGameMode/DA_SurvivalModeConfig`，把 `Progression → Buff Pool` 指向 `DA_BuffOfferPool`。服务器升级时 `UCC_TeamProgressionComponent` 已为每位玩家调用 `OfferBuffs`，已有一轮未选完则排队。确认当前 GameMode 使用了这份配置即可，不要再在 `CombatMain` 的经验条更新事件里调用 `OfferBuffs`，否则会重复发放。

**手动测试或其他玩法：** 在服务器端蓝图（例如 GameMode 的测试事件）中取得目标 `PlayerController`，调用静态节点 `Find For Player`（目标为该 Controller），检查返回的 `CC_BuffSelectionComponent` 有效，再调用其 `Offer Buffs`，`Pool` 指向 `DA_BuffOfferPool`。该节点标记为 `BlueprintAuthorityOnly`，不要从客户端 Widget 调用。`Offer Buffs` 返回 false 时先检查池和条目。需要自己指定选项时才用 `Offer Options`。

```text
服务器蓝图：目标 PlayerController
  → Find For Player(Player)
  → Is Valid?
  → Offer Buffs(Pool = DA_BuffOfferPool)
  → 检查返回值
```

选项复制到拥有者后，组件会自动打开 `WBP_BuffSelection`；卡片点击调用 C++ 选择逻辑，服务器验证、施加 GE，最后自动换下一轮或关闭页面。

### 6. PIE 验收顺序

1. 单机 PIE，服务器端手动发一轮，确认出现 3 张不同卡片，名称/图标来自对应 GE。
2. 点一张，确认只施加一次 Buff，卡片在提交时不可重复点击；没有排队时页面自动关闭。
3. 连续发两轮，确认第二轮在选完第一轮后出现；提升经验触发升级时确认只发预期轮数。
4. 多人 PIE，在客户端各自选择：每人只看到自己的卡，且只有服务器施加 Buff。

**常见故障**

| 现象 | 先检查 |
|---|---|
| 不弹窗 | `BP_CC_PlayerController.BuffSelection.Selection Screen Class`、实际 GameMode 的 Controller 类、`Offer Buffs` 返回值、Root 是否就绪。 |
| 弹窗但没有卡片 | `WBP_BuffSelection` 是否继承正确父类；`PanelWidget_Cards` 名字/类型；`Card Class` 是否指向编译成功的 `WBP_BuffOfferCard`；该轮是否确实有有效选项。 |
| 卡片文字/图标为空 | GE 是否带 `CC Buff UI Data` 且填了字段；卡片三个 `BindWidget` 控件是否匹配。 |
| 卡片不可点击 | 页面是否被更高层遮挡、卡片区是否被设置为全子树不可命中；提交中的卡片本来就会暂时禁用。 |
| 升级后不出现选择 | `DA_SurvivalModeConfig.Progression.BuffPool` 是否已指定；是否真的升了级；池里是否有有效条目。 |

## 此前说明的疏漏与本次修正

- 之前只列了“做卡片/做页面”的思路，没有逐段说明正在编辑哪个蓝图。本版把 `WBP_BuffOfferCard`、`WBP_BuffSelection`、`BP_CC_PlayerController` 和两份数据资产的操作分开，避免把事件接到错误的蓝图。
- 之前没有突出 `BindWidget` 与自动生成的边界：卡片三个控件的名称/类型是必需的；页面的 `PanelWidget_Cards` 在手工布局时可省，但**自动生成方案**必须放；`Card Class` 也必须指定。
- 之前容易让人以为要在经验条或关卡蓝图里手动 `Create Widget`。本版明确：组件已负责收到 Offer 后自动 `ShowScreen`，生存模式升级也已负责发 Offer；蓝图只需配类和池。
- 之前提到了 `Choose Option`，却没区分自定义卡片和现有卡片的路径。默认卡片的点击已由 C++ 实现，不要再接一条蓝图提交线；仅完全自定义页面时才自己调用 `Choose Option(OptionIndex)`。
- 之前的故障信息不够可验证。本版按“池是否抽到有效项 → 组件是否配置页面 → 页面是否生成卡片 → 点击是否被接收”给出 PIE 检查顺序；文中节点和字段均来自现有 C++ 接口，没有要求创建或调用未实现的函数。

## 行为说明

- **暂停**：页面在 GameMenu 层，单机打开时根布局会暂停世界；联机不会暂停，选的时候角色仍会受到攻击。
- **不能用返回键关闭**：必须选一张。如果需要"跳过"，可以在服务器调用 `Clear Offers`。
- **死亡**：选择时没有存活角色会返回 `NoTarget`，本轮保留，重生后可以继续选。三选一挂在控制器上，重生不会打断。
- **防连点**：提交后到服务器回结果前，卡片不可点击；针对旧一轮的请求会被服务器拒绝。
- **其他页面盖在上面时**：选择组件会等页面可以打开时再打开（重试不受暂停影响）；暂停菜单关闭后三选一仍在。

## 测试

- `GAS_Demo.Buff.Selection.Roll`：抽取数量、同轮不重复、跳过无效条目、池不足时全部给出。
- `GAS_Demo.Buff.Selection.ServerFlow`：排队、编号与下标校验、施加失败保留本轮、成功进入下一轮、重复请求拒绝、没有角色时保留、清空。

未覆盖、需要实机验证：页面自动打开与关闭、手柄焦点、联机客户端的复制与 RPC、单机暂停。

## 以后可能扩展

需要时再加，当前不实现：权重与稀有度、已选次数上限、重抽、跳过按钮、所有玩家共享同一轮。

## 服务器事件（玩法规则层订阅）

- `OnServerPendingChanged(bool bHasPending)`：待选的一轮从无到有、从有到无时广播（排队中的下一轮接上时不广播）。生存模式用它在联机选卡期间施加保护 GE。
- `OnServerChoiceAccepted(Option)`：选择被 GAS 接受时广播。生存模式用它统计已选 Buff 数。

两者都是原生委托，只在服务器广播；组件本身仍不认识任何具体 Buff。测试：`GAS_Demo.Buff.Selection.ServerEvents`。
