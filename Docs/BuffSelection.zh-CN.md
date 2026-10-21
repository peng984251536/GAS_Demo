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

## 编辑器接线（你需要做的）

1. **编译 C++ 并重启编辑器。**
2. **建 Buff 池**：内容浏览器 → 右键 → 杂项 → 数据资产 → 选 `CC_BuffOfferPool`，例如 `DA_BuffPool_Default`。
   - `Entries` 里填 Buff 的 GE 蓝图和等级。**GE 必须带 `CC Buff UI Data` 组件**（名称、描述、图标都从这里读），否则会被跳过，数据校验会给出警告。
   - `Offer Count` 是每轮给几个，默认 3。
3. **做卡片蓝图**：新建控件蓝图，父类选 `CC_BuffOfferCardWidget`，例如 `WBP_BuffOfferCard`。
   - 必须放名为 `TextBlock_Name`、`TextBlock_Description`（TextBlock）和 `Image_Icon`（Image）的控件，会自动填充；缺少同名、同类型控件时蓝图编译报错。
   - 其他表现（边框、悬停动画、等级显示）在 `On Card Data Set` 事件里自己做，数据里有 `Effect`、`Level` 等字段。
   - 按钮的悬停/按下样式按 CommonButton 的方式配置。
4. **做页面蓝图**：新建控件蓝图，父类选 `CC_BuffSelectionScreen`，例如 `WBP_BuffSelection`。
   - 放一个名为 `PanelWidget_Cards` 的面板（HorizontalBox 等），类默认值里把 `Card Class` 设为第 3 步的卡片蓝图，卡片会按每轮数量自动生成，默认焦点在第一张。
   - 想完全自己摆放也可以不放 `PanelWidget_Cards`，在 `On Buff Offer Changed` 里根据 `Cards` 更新自己的控件，点击时调用 `Choose Option(OptionIndex)`。
   - `On Buff Offer Changed` 的 `bSubmitting` 可用来显示"等待中"，`Last Result` 不为 None 时可提示失败（例如 NoTarget = 角色已死亡）。
5. **在 PlayerController 蓝图里设置界面**：打开你的 PlayerController 蓝图（父类为 `CC_PlayerController`），选中组件 `BuffSelection`，把 `Selection Screen Class` 设为第 4 步的页面。`Screen Layer` 默认 `UI.Layer.GameMenu`，一般不用改。
6. **在需要的地方发起三选一**（只能在服务器执行，例如 GameMode、敌人刷新器、关卡蓝图）：

   ```text
   Get Player Controller (或遍历所有玩家)
   → Find For Player（或 Cast 到 CC_PlayerController → Get Buff Selection）
   → Offer Buffs (Pool = DA_BuffPool_Default)
   ```

   连续调用多次会排队，玩家选完一轮自动进入下一轮。需要自定义抽取规则时用 `Offer Options` 直接传选项数组。

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
