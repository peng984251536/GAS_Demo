# Buff 三选一

肉鸽式的 Buff 选择：服务器从 Buff 池里随机抽几个选项（默认 3 个），玩家选一个，服务器施加对应的 GE。联机时每个玩家各自抽、各自选。

升级后只增加待选奖励次数并通知战斗主界面显示红点；点击主界面的按钮后才打开三选一。按钮与红点的布局、尺寸和样式由 Widget 蓝图负责；控件按约定命名后，点击、启用和显隐由 C++ 自动处理。

本系统只负责 Buff 的"获取"（见 [BuffSystem.zh-CN.md](BuffSystem.zh-CN.md) 第 3 节）。选中之后就是一次普通的 `UCC_BuffComponent::ApplyBuff`，叠层、免疫、持续时间都由 GE 自己决定。

## 类一览

| 类 | 位置 | 职责 |
|---|---|---|
| `UCC_BuffOfferPool` | `Data/Buff/` | 数据资产：候选 Buff 列表 + 每轮数量；运行时只读 |
| `UCC_BuffSelectionComponent` | `Components/` | 挂在 PlayerController 上。服务器抽取、排队、校验、施加并同步待选次数；客户端主动请求打开界面 |
| `UCC_CombatMainController / Model` | `UI/Combat/` | 监听奖励变化，发布待选次数；蓝图根据模型显示红点，点击后转发打开请求 |
| `UCC_BuffSelectionScreen` | `UI/BuffSelection/` | 页面基类（`CC_ActivatableWidget`），自动生成卡片，选完自动关闭 |
| `UCC_BuffSelectionController` / `Model` | `UI/BuffSelection/` | 监听选择组件，把当前一轮转成卡片快照；转发点击 |
| `UCC_BuffOfferCardWidget` | `UI/BuffSelection/` | 单张卡片，继承 `CommonButtonBase`，点击即选择 |

## 流程

```text
服务器  OfferBuffs(Pool)
          ├─ Pool.RollOptions：过滤无效条目 → 不放回随机抽 OfferCount 个（同一 GE 一轮只出现一次）
          └─ 分配 OfferId；已有一轮未选完 → 排队，否则设为当前一轮
               ↓ 复制（仅拥有者）
客户端  OnRep → OnOfferChanged → CombatMainModel 更新待选次数 → 蓝图显示红点
          玩家点击按钮 → OpenBuffSelection → RequestOpenSelection → ShowScreen(GameMenu 层)
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
| `WBP_BuffSelection`（新建） | 页面布局、指定卡片类、可选状态提示 | 生成卡片、禁止重复提交、自动关闭（CommonButtonBase_Close 由基类绑定） |
| `BP_CC_PlayerController` | 给 `BuffSelection` 组件指定页面类 | 玩家点击入口后在 UI 层打开页面 |
| `WBP_CombatMain` | 按第 6 节放置同名的按钮和红点控件 | 绑定按钮点击、按次数切换红点显隐 |
| `DA_SurvivalModeConfig` | 升级时自动发放待选奖励，配置 `Progression.BuffPool` | 升级后向每个玩家发放选项 |

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
   ├─ TextBlock_Result         [可选：失败提示/提交状态]
   └─ CommonButtonBase_Close             [可选：CommonButton 子类（如 CC_ButtonBase 的蓝图），关闭页面]
```

将 HorizontalBox **准确命名为 `PanelWidget_Cards`** 并勾选 `Is Variable`。它是可选绑定字段，但最小自动生成方案必须放它；不放时 C++ 不会创建任何可见卡片。给面板的子项设置间距、居中对齐，保证 1～3 张卡片也能正常排列。背景要挡住下层鼠标点击，卡片区不可设置为 `Not Hit-Testable (Self & All Children)`。

关闭按钮**准确命名为 `CommonButtonBase_Close`**，类型必须是 CommonButton（继承 `CommonButtonBase` 的按钮蓝图，建议用 `CC_ButtonBase` 子类以带音效）。这个绑定由通用基类 `CC_ActivatableWidget` 提供，所有页面都能用，C++ 自动绑定点击，点击效果与返回键相同：关闭页面，不消耗奖励。不放它时仍可用返回键关闭。不要再给它接 OnClicked → Close，否则会关两次（第二次无效，但没必要）。

**在 `WBP_BuffSelection` 的 Class Defaults 中**设置 `Card Class = WBP_BuffOfferCard`。保留父类的 `Controller Class = CC_BuffSelectionController`、`Input Config = Menu` 和 `Allow Back = true`；误清空 `Controller Class` 会使模型为空。编译并保存。

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

选项复制到拥有者后，组件只通知主界面显示红点。点击入口后打开 `WBP_BuffSelection`；卡片点击调用 C++ 选择逻辑，服务器验证、施加 GE，最后自动换下一轮或关闭页面。

### 6. 在 WBP_CombatMain 放入口按钮和红点

布局、尺寸和样式由你决定；只要控件**名字和类型**对上，C++ 会自动接线，蓝图里不需要连任何节点。三个控件都是可选绑定，不放也能编译。

```text
WBP_CombatMain
└─ （任意位置）
   ├─ CommonButtonBase_BuffSelection        [CommonButton 子类，例如继承 CC_ButtonBase 的按钮蓝图]
   │  └─ ……按钮自己的图标/文字
   ├─ Widget_BuffRedDot           [任意控件，例如 Image 红点；一般叠在按钮右上角]
   └─ TextBlock_BuffRewardCount   [可选：TextBlock，显示待选次数]
```

C++ 的行为：

| 控件 | 待选次数 > 0 | 待选次数 = 0 |
|---|---|---|
| `CommonButtonBase_BuffSelection` | 点击打开三选一 | 仍可点击，但不打开页面 |
| `Widget_BuffRedDot` | 显示（不挡点击） | 折叠 |
| `TextBlock_BuffRewardCount` | 显示次数 | 折叠 |

注意：

- 按钮必须是 CommonButton。普通 UMG `Button` 类型不匹配，不会绑定。
- 红点叠在按钮上时，C++ 已设为 `HitTestInvisible`，不会挡按钮点击。
- 如果以前已经在蓝图里手动接了按钮 OnClicked 或在 On UI Model Changed 里控制红点，请删掉，避免重复。
- 要加动画（例如红点出现时跳一下），实现 `Event On Buff Rewards Changed(PendingCount)`，它只在次数变化时触发。
- 手柄按键入口还没做：主界面不参与焦点，手柄无法聚焦这个按钮。需要时给某个输入动作调用 `Open Buff Selection` 即可。

Model 保存次数快照，实际次数由服务器现有队列决定：成功选中后才减一次，失败不减，连续升级可累积。界面重新激活时主动读取最新次数，因此关闭重开或重生不会漏通知，也不需要 Model 单独累加。

### 7. PIE 验收顺序

1. 单机 PIE，服务器端手动发一轮或升级，确认只显示红点、不弹窗；点击主界面按钮后出现卡片，名称/图标来自对应 GE。
2. 点一张，确认只施加一次 Buff，卡片在提交时不可重复点击；没有排队时页面自动关闭。
3. 连续发两轮，确认模型待选次数为 2，选完第一轮后变为 1 并保留红点；全部选完后次数归零、红点消失。提升经验触发升级时只发预期轮数。
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
- 组件负责点击入口后的 ShowScreen，生存模式升级负责发 Offer；主界面蓝图只读取待选次数并转发按钮点击，不重复发放奖励。
- 之前提到了 `Choose Option`，却没区分自定义卡片和现有卡片的路径。默认卡片的点击已由 C++ 实现，不要再接一条蓝图提交线；仅完全自定义页面时才自己调用 `Choose Option(OptionIndex)`。
- 之前的故障信息不够可验证。本版按“池是否抽到有效项 → 组件是否配置页面 → 页面是否生成卡片 → 点击是否被接收”给出 PIE 检查顺序；文中节点和字段均来自现有 C++ 接口，没有要求创建或调用未实现的函数。

## 行为说明

- **暂停与保护**：红点提醒期间继续游戏。点击后页面在 GameMenu 层打开，单机由根布局暂停；联机不暂停，生存模式仅在页面实际激活时授予配置的选卡保护 GE，失活或选完时撤销。
- **返回键或 `CommonButtonBase_Close` 可关闭**：页面是玩家主动打开的，直接关闭，不消耗奖励，红点继续显示，下次点击入口回到同一轮（选项不会重抽）。关闭时同步撤销选卡保护。提交中关闭不影响结果，服务器照常处理。如果要彻底放弃奖励，可以在服务器调用 `Clear Offers`。
- **死亡**：选择时没有存活角色会返回 `NoTarget`，本轮保留，重生后可以继续选。三选一挂在控制器上，重生不会打断。
- **防连点**：提交后到服务器回结果前，卡片不可点击；针对旧一轮的请求会被服务器拒绝。
- **其他页面盖在上面时**：主界面 Controller 拒绝下层点击；只有主动点击发起的打开请求才会在根布局未就绪或过渡时重试，升级通知不会自动开页。

## 测试

- `GAS_Demo.Buff.Selection.Roll`：抽取数量、同轮不重复、跳过无效条目、池不足时全部给出。
- `GAS_Demo.Buff.Selection.ServerFlow`：排队与待选次数、编号与下标校验、失败不扣次数、成功进入下一轮、重复请求拒绝、打开失败保留奖励、页面会话结束时撤销保护通知、清空。

需要放好控件后实机验证：入口按钮与红点、关闭按钮、点击打开与自动关闭、手柄焦点、联机客户端的复制与 RPC、单机暂停。

## 以后可能扩展

需要时再加，当前不实现：权重与稀有度、已选次数上限、重抽、跳过按钮、所有玩家共享同一轮。

## 服务器事件（玩法规则层订阅）

- `OnServerPendingChanged(bool bHasPending)`：待选的一轮从无到有、从有到无时广播（排队中的下一轮接上时不广播）。
- `OnServerSelectionActiveChanged(bool bActive)`：页面激活/失活或奖励全部选完时广播，生存模式用它管理选卡保护；仅待选时不会授予保护。
- `OnServerChoiceAccepted(Option)`：选择被 GAS 接受时广播。生存模式用它统计已选 Buff 数。

这些都是原生委托，只在服务器广播；组件本身仍不认识任何具体 Buff。测试：`GAS_Demo.Buff.Selection.ServerEvents`、`ServerFlow`。
