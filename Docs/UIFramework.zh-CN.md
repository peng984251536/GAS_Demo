# Lyra 第一阶段 UI 框架学习版

本框架现已接入独立 Controller + 展示 Model。主菜单、HUD 和暂停示例的最新职责与蓝图接线以 [UIArchitecture.zh-CN.md](UIArchitecture.zh-CN.md) 为准；本文描述底层 CommonUI 容器及输入规则。

## 范围与当前状态

当前实现以本机 UE 5.6 官方 Lyra 为对照，采用 UIManager → UIPolicy → 每个 LocalPlayer 的 RootLayout。旧 Controller 启动和房间强制重建流程已删除；新接入步骤见 [生命周期与接入说明](LyraUI-Phase1.zh-CN.md)。

更早一轮迁移的备份仍在 `Saved/UIFrameworkMigration/Backup`，不是本次改造的回滚快照。本阶段不引入 Experience、UIExtension、CommonUser 或 MVVM。官方类与项目类的对应、学习顺序和保留差异见 [LyraUI-Phase1.zh-CN.md](LyraUI-Phase1.zh-CN.md)。

## 类的职责

| 类 | 作用 |
| --- | --- |
| `UCC_UIManagerSubsystem` | GameInstance 级服务，创建 Policy 并转发本地玩家和世界生命周期 |
| `UCC_UIPolicy` | 按 LocalPlayer 持有根布局，选择布局类型并挂载玩家视口 |
| `UCC_RootLayout` | Game/GameMenu/Menu/Modal 四层官方栈、通知层、层级事件、暂停及输入保护 |
| `UCC_AsyncShowScreen` | 软引用异步打开页面，等待动画结束，支持取消与切图清理 |
| `UCC_UIStack` | 对官方 CommonActivatableWidgetStack 的薄封装，设置动画和对象池策略 |
| `UCC_ActivatableWidget` | 页面基类：输入模式、返回策略、默认焦点、上下文注入、统一关闭 |
| `UCC_UIInputData` | 官方 InputData 工作流的原生默认动作表：Enter/A 确认，Esc/B 返回 |
| `UCC_PlayerHUDWidget` | HUD 展示层，读取 PlayerHUDModel；GAS 订阅和 Pawn 切换由 PlayerHUDController 负责 |
| `UCC_UIController / UCC_UIModel` | 页面业务协调器与展示模型基类，随页面自动绑定/解绑 |
| `UCC_MainMenuController / UCC_MainMenuModel` | 主界面统一操作入口和展示状态，已接房间子系统，开始/继续提供存档业务扩展点 |
| `UCC_MenuButton / UCC_PauseMenuWidget / UCC_QuitDialogWidget` | 原生交互示例，分别演示 CommonButton、菜单和确认框；可由自己的蓝图替换 |
| `UCC_WidgetComponent / UCC_AttributeWidget` | 原有世界空间血条，仍使用普通 UMG，补齐最大值更新和委托解绑 |

UI 不要求特定 PlayerController 子类；项目控制器直接继承 APlayerController。

## 层级与交互

视觉顺序从下到上：

1. `UI.Layer.Game`：HUD。HUD 不响应返回，也不抢焦点。
2. `UI.Layer.GameMenu`：背包、记分板等玩法界面。
3. `UI.Layer.Menu`：主菜单、暂停、设置。同层仅显示栈顶，下面页面失活并保留在栈中。
4. `UI.Layer.Modal`：确认框。显示时下层仍可见，但不能被鼠标点击；输入和焦点由 CommonUI 交给顶层页面。
5. 通知容器：只显示普通 UserWidget，自身和子控件都不参与命中测试，不抢输入。

`UI.Layer.GameMenu` 现在拥有独立栈，不再是 Menu 别名。不同层的下层页面不必失活；例如 HUD 可继续接收数据事件。

根布局默认在单机打开 GameMenu/Menu/Modal 时暂停世界，在最后一个菜单退出并结束过渡后恢复。仅撤销本 UI 自己施加的暂停。实时背包项目可在根布局类默认值关闭 `Pause Game While Menu Open`。输入模式与世界暂停是两件事；关闭自动暂停后仍可通过页面的 Menu 输入配置阻止角色操作。

## 蓝图接入步骤

1. 编译 C++ 并重启编辑器。任意本地 PlayerController 均可使用 UIManager 查询布局。
2. 根布局只由 UIPolicy 配置，地图初始页面由独立装配入口打开，不再读取 Controller 的 HUD/初始菜单属性。
3. 新建主菜单使用 `CC_MainMenuWidget`；背包、设置和确认框使用 `CC_ActivatableWidget`，复杂页面通过 ControllerClass 配置独立控制器。已经接入 CC 框架的页面无需重新改父类；旧框架页面应先核对绑定，再逐项迁移导航，不要批量删除业务节点。
4. 自定义玩家 HUD 的父类改为 `CC_PlayerHUDWidget`，实现 `On Vitals Changed` 更新自己的血蓝条。设计器有 WidgetTree 时保留蓝图布局，不生成原生示例布局。
5. 根布局可直接用原生 `CC_RootLayout`，无需手工创建栈或 RegisterLayer。若要调动画，可派生根布局蓝图，只修改类默认值；根布局运行时自行构建容器，不使用设计器中的自定义根树。
6. 通知使用普通 `UserWidget`，世界空间血条仍使用原来的 `CC_AttributeWidget` 和 `CC_WidgetComponent`。

通用页面打开：

```text
Get Owning Player → Get Root Layout For Player
→ Root.Show Screen
    Layer = UI.Layer.GameMenu / UI.Layer.Menu / UI.Layer.Modal
    Screen Class = 你的页面蓝图
    Context = 可选业务数据 UObject
```

页面关闭：调用页面自身的 `Close Screen`。需要从外部关闭时：`Get Root Layout For Player → Close Top Screen`。栈内页面不要使用 AddToViewport、RemoveFromParent 或自己修改 ZOrder/InputMode。

同层同类页面不重复创建，返回已有实例，也不自动把被覆盖页面移到最前面。过渡或同步栈修改期间的新打开请求返回空，关闭返回 false；不排队，需要调用方处理返回值。正常按钮在过渡期间被输入过滤和遮罩保护。

异步打开使用 `Show Screen Async`：RootLayout 接 Get Root Layout For Player，ScreenClass 传软类引用，Context 是可选上下文，默认 SuspendInput=true。Completed 输出已入栈的 Screen（不保证入场动画已结束）；Failed 表示无效层、类或加载失败。保存 AsyncAction 输出可以调用 Cancel；取消不发成功/失败事件。根布局 Shutdown 会立即取消挂起请求。加载完成遇到过渡时会等待，多个请求按就绪顺序处理，不保证发起顺序。关闭仍统一使用 CloseScreen，由 CommonUI 管理退出动画。

通知：`Get Root Layout → Show Notification(普通 Widget 类, Duration)`，可用返回的控件调用 `Dismiss Notification`。计时使用游戏时间，暂停游戏时也暂停计时；Duration <= 0 表示手动关闭。多个通知共享全屏 Overlay，具体排版由通知蓝图负责。

## 生命周期与事件绑定

| 回调 | 适合做什么 |
| --- | --- |
| On Initialized | 每个 UObject 实例只执行一次的设置，如绑定自己的按钮事件 |
| On Screen Opened | 每次 ShowScreen 入栈前接收 Context；重置对象池中上次使用留下的业务状态 |
| On Activated | 绑定展示期间需要的事件，并主动读取一份初始数据；返回本页时也会触发 |
| On Deactivated | 解除展示期间的订阅；可能只是被同层另一页面覆盖，不等于永久关闭 |
| Destruct | Slate 释放或对象池回收阶段的补充清理，不等于 UObject 立刻销毁 |

`On Screen Opened` 只负责数据初始化，不要在其中同步再次打开/关闭页面，否则会被重入保护拒绝。`ScreenContext` 出栈释放 Slate 时会清空。对象池由 CommonUI 自己管理，不要缓存页面实例来重新 AddToViewport。

关闭先失活，由官方栈播放退出过渡后释放/复用控件。不要在 On Deactivated 自己播放一段延迟动画再 RemoveFromParent，否则会与栈的生命周期冲突。通知不是页面栈控件，其到期移除是即时的；如需专用通知进出动画，可后续单独扩展通知控件。

## 输入和焦点

- 页面 `Input Config`：Game、GameAndMenu、Menu 三种声明式模式。
- 主菜单底页可设 `Allow Back=false`，`Allow Close=true`：返回键不能关闭，但开始按钮仍能主动关闭。
- 永久 HUD 使用 `Allow Close=false`，不参与焦点导航。
- 页面实现 `Get Desired Focus Target` 返回默认按钮；也可设置 `Default Focus Widget Name`。
- `On Handle Back Action` 返回 true 表示蓝图已处理此次返回；例如先弹未保存确认框。
- 首次打开暂停菜单由玩法输入动作显式调用 Root.ShowScreen；不再硬编码 Esc/Start。已有菜单时 Esc/B 由 CommonUI 处理返回。
- UI 使用 CommonUI 原生动作表，游戏移动/攻击继续使用原有 Enhanced Input；未开启 CommonUI 的 Enhanced Input 集成。

## 动画

根布局提供 `Transition Duration`、`Menu Transition`、`Modal Transition`。类型来自官方 switcher：FadeOnly、Horizontal、Vertical、Zoom。默认菜单横移、弹窗淡入淡出，时长 0.18 秒，Game 层即时显示。

动画期间按容器保存输入过滤令牌，并覆盖透明点击遮罩；动画结束后解除相应过滤、刷新焦点。切图中断动画时 Shutdown 统一释放令牌。按钮悬停、血条受击、页面内部装饰动画仍在对应蓝图中实现。

## 数据更新与后续验证

HUD 的更新链：Pawn 变化 → PlayerHUDController 绑定 ASC → 四项属性写入 PlayerHUDModel → Widget 收到模型通知 → On Vitals Changed。失活/重生时控制器精确解绑旧句柄；世界空间血条也同时监听当前值与最大值，避免只改 MaxHealth 时界面不刷新。

编译后建议实际验证：打开暂停 → 退出确认 → 取消 → 返回游戏；手柄焦点是否恢复；连续点击是否重复入栈；血量和最大血量变化；重生换 Pawn；菜单动画途中切图；再次进入关卡没有重复 HUD。键盘 Esc 在 PIE 中可能优先触发编辑器停止运行，请在独立游戏窗口验证完整返回流程。

## 参考依据

- Epic CommonUI Quickstart：ViewportClient、确认/返回动作及输入配置。
- Epic Input Fundamentals：GetDesiredInputConfig、默认焦点和焦点恢复。
- Epic CommonUI Design Guidelines：页面使用 ActivatableWidget，世界空间 UI 保留普通 WidgetComponent。
- 本机 UE 5.6 的 CommonActivatableWidgetContainer / SCommonAnimatedSwitcher 源码，作为实际接口与过渡行为依据。

官方文档：https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine
