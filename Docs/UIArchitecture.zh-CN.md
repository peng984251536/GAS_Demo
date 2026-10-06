# UI 架构：View、Controller 与展示 Model

这里描述项目已有的独立业务控制层。UI 基础设施已按 Lyra 第一阶段迁移为 UIManager → UIPolicy → LocalPlayer RootLayout；业务 Controller/Model 继续保留，它们是本项目设计，不代表官方 Lyra 的统一数据架构。迁移对照见 [LyraUI-Phase1.zh-CN.md](LyraUI-Phase1.zh-CN.md)。

## 职责与所有权

| 层 | 项目类 | 职责 |
| --- | --- | --- |
| 基础设施 | CC_UIManagerSubsystem、CC_UIPolicy、CC_RootLayout | 本地玩家布局所有权、四层栈、输入、焦点和动画 |
| View | CC_ActivatableWidget、CC_MainMenuWidget、CC_MultiplayerScreenWidget、CC_PlayerHUDWidget | 布局、展示、动画；把操作转发给 Controller |
| Controller | CC_UIController 及功能子类 | 订阅业务系统、协调请求、导航、转换展示数据 |
| 展示 Model | CC_UIModel、CC_MainMenuModel、CC_PlayerHUDModel | 保存 UI 快照，通过 OnChanged 通知界面 |
| 真实业务 | GAS、CC_OnlineRoomSubsystem 等 | 验证规则、保存真实数据、完成跨地图事务 |

页面强引用 Controller，Controller 强引用 Model；Controller 只弱引用页面和业务系统。Model 不引用 Widget 或 Controller。主菜单和 HUD 模型的写入口只向各自 C++ Controller 开放；View 使用 Getter 获取快照。

这是一套带独立控制层的展示架构。UIModel 是展示模型，不能代替 InventoryComponent、AttributeSet 或 SaveGame。这里的 UObject Controller 也不是 UE 的 APlayerController。

## 自动生命周期

1. `ShowScreen → PrepareForDisplay`：结束旧的入栈会话，创建 Controller/Model、传入 Context，然后触发 On Screen Opened。
2. 页面激活：订阅 Model.OnChanged，激活 Controller；Controller 订阅系统并读取快照；View 得到初始刷新，然后执行页面蓝图 On Activated。
3. 页面失活：解除 View 的模型订阅，Controller 作废激活令牌、解除系统监听，然后执行官方失活流程。
4. 同层另一页关闭后恢复：保留原 Controller/Model，重新订阅系统并读取最新快照。
5. 页面释放 Slate：兜底清理 Controller，清空页面上下文；下次入栈使用新会话。即使池化对象上一帧 Slate 尚未释放，PrepareForDisplay 也会主动重建控制器。

跨层 Modal 打开时，下面的 Menu 可能仍保持激活，因此仍能接收模型更新；`CanHandleActions` 会检查它是不是全局顶层，阻止下层 Controller 执行用户操作。控制器初始化、激活和模型刷新事件中只初始化/刷新数据，不同步执行页面导航。

## 主界面蓝图接线

1. 主菜单 WBP 改为继承 `CC_MainMenuWidget`，保留自己的设计器布局。
2. 创建继承 `CC_MainMenuController` 的蓝图，例如 BP_MainMenuController。在 WBP 默认值中把 `Controller Class` 设为它。
3. BP_MainMenuController 默认值设置 `Settings Screen Class`；退出确认默认使用原生示例，可替换。
4. 主菜单通过 `SearchRooms` 发起搜索并打开房间列表，通过 `LeaveCurrentRoom` 清理会话；其他按钮仍调用主菜单控制器的设置、退出和开始接口。
5. 房间列表蓝图继承 `CC_MultiplayerScreenWidget`；它从 `On Room List State Changed` 显示搜索结果，每一行通过 `JoinRoom(RoomId)` 加入对应房间。
5. 实现 WBP 的 `On UI Model Changed`：将 Model 转成 CC_MainMenuModel，调用 Get State，更新房间列表、加载提示、按钮可用性与错误信息。不要在这个展示回调里重新发起搜索。
6. 主菜单默认禁止返回键关闭；默认焦点继续通过 Get Desired Focus Target 或 Default Focus Widget Name 配置。
7. 初始页面由地图装配入口通过 UIManager 的 OnRootLayoutReady 和 Get Root Layout For Player 打开，不再使用 Controller 的初始页面配置。

`bBusy` 用于显示忙碌和禁用重复操作，`bRoomServiceAvailable` 用于服务可用性，`RoomState` 用于决定当前可用操作。按钮禁用只是展示，业务子系统仍会验证请求。RecoveryRequired 表示需要调用 LeaveRoom 清理残留会话。

`LastRoomResult` 是房间服务最近结果；默认结果的空 Message 不应显示为空错误弹窗。`ActionError` 是主菜单自身的设置页面/开始流程错误。建议分别展示，避免把旧房间错误误认为刚刚发生的新错误。

## 开始游戏、继续游戏与异步请求

源码尚未提供存档业务 API，本轮保留明确扩展点，不假定存档槽或目标地图。

在 BP_MainMenuController 覆盖 `Execute Start Game(RequestId, bContinue)`，调用你的存档/加载系统。结束时把**发起时保存的 RequestId**传给 `Complete Start Game`。默认实现会报告尚未接入业务，而不会伪装成功。RequestStartGame 返回 true 只表示已进入处理流程，不能当作加载成功。

重复点击会在请求未完成时被拒绝；失活会作废 RequestId；迟到或重复 CompleteStartGame 返回 false。C++ 异步回调应捕获弱控制器引用；其他功能 Controller 可使用 GetActivationToken / IsActivationCurrent 实现同样的页面会话校验。调用和结果提交均在游戏线程执行。

废弃 UI 请求不等于取消存档或网络任务。若底层流程需要跨页面、跨地图继续，交给 GameInstanceSubsystem 等业务对象持有，页面恢复后读取其快照。若任务应当取消，在 ReceiveDeactivated 中显式调用业务服务提供的取消接口。Controller 不会凭空取消不了解的底层任务。

房间的搜索、建房、加入、退出已经实际接上 CC_OnlineRoomSubsystem。控制器失活只解除订阅，房间事务继续由子系统管理。主菜单恢复时重新读取状态、缓存列表和最近结果。

## HUD 和示例页面

HUD 现在为 `GAS → CC_PlayerHUDController → CC_PlayerHUDModel → CC_PlayerHUDWidget`。Controller 管 Pawn 变化、ASC 就绪和四项属性委托，Model 管血蓝快照，Widget 只画进度和文字。原来的蓝图 OnVitalsChanged 继续保留。新增 HUD Controller 默认已由原生 Widget 指定，不需要手工实例化。

暂停与退出确认的按钮也改为调用各自 Controller。普通按钮、通知和世界空间血条不强制增加控制器。未来背包可仿照 HUD 新建 InventoryController + InventoryModel，不在 RootLayout 中增加背包规则。

新增复杂页面时：定义展示模型 → 定义功能 Controller → 在页面设置 ControllerClass → View 转发操作并实现 OnUIModelChanged。功能控制器的 C++ OnActivated/OnDeactivated 覆盖应调用 Super，保证蓝图扩展事件执行。

## 验证清单

本轮只进行了源码静态检查，未编译、未执行自动化或 PIE。

- 编辑器自动化测试 `GASDemo.UI.Controller.LifecycleIsolation`：检查重复激活、令牌过期、恢复页面保留模型、重新打开模型隔离。
- 暂停 → 退出确认 → 取消 → 继续；下层按钮不能绕过弹窗执行。
- 主菜单搜索房间；连续点击无重复操作；失败时恢复忙碌状态和错误展示。
- 打开设置再返回主菜单，能读取最新房间状态，监听不重复。
- 开始请求完成两次、页面关闭后完成、关闭重开后旧请求完成，都应拒绝旧回调。
- HUD 当前值/最大值变化、重生换 Pawn、地图切换之后，数据正确且不重复订阅。

原 CommonUI 层级、动画和输入配置见 [UIFramework.zh-CN.md](UIFramework.zh-CN.md)。
