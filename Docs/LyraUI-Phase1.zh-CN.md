# Lyra UI 第一阶段：本地玩家驱动的生命周期

本版已删除旧项目的 Controller 启动/重建 UI 流程。UI 框架不要求 BP_CC_PlayerController，也不再提供 CC_UIPlayerController 兼容父类。

## 核心思路

GameInstance 创建 UIManager → Manager 监听引擎 LocalPlayer 加入与 ControllerChanged → UIPolicy 按 LocalPlayer 创建一次 RootLayout → 四个 CommonUI 栈承载页面。

根布局属于本地玩家，不属于地图或 Controller。地图变化时清空页面、取消异步导航、释放输入令牌和页面对象池，脱离视口；到达新地图后更新玩家上下文并挂载同一根布局。只有本地玩家退出或 GameInstance 结束才永久 Shutdown。

“同一根布局”不代表保留所有旧页面：HUD、业务 Controller/Model、通知计时器和地图上下文不能无条件跨地图存活。

## 官方对应

| Lyra | 本项目 |
| --- | --- |
| GameUIManagerSubsystem | CC_UIManagerSubsystem |
| GameUIPolicy | CC_UIPolicy |
| PrimaryGameLayout / W_OverallUILayout | CC_RootLayout |
| CommonActivatableWidgetStack | CC_UIStack |
| LyraActivatableWidget | CC_ActivatableWidget |
| AsyncAction_PushContentToLayerForPlayer | CC_AsyncShowScreen |

官方通过 CommonGameInstance/CommonLocalPlayer 通知策略；此版本直接使用 UE 5.6 的 OnLocalPlayerAddedEvent、OnPlayerControllerChanged、OnLocalPlayerRemovedEvent，不需要自定义 PlayerController 或复制完整 CommonGame 插件。PostLoadMapWithWorld 覆盖普通及无缝切图完成，OnWorldBeginTearDown 清理旧世界内容。

## 新的使用方式

1. 重启编辑器加载新增/删除的反射类型。GameMode 继续选择适合玩法的 Controller，UI 不限制其父类。
2. 新页面继承 CC_ActivatableWidget，菜单配置 Input Config=Menu，设置 Get Desired Focus Target 返回可聚焦按钮。
3. 蓝图通过 Get Root Layout For Player（CC_UIManagerSubsystem 的静态节点）传入 Get Owning Player / 本地 PlayerController。
4. 在返回的 Root 上调用 Show Screen，传页面类和 Layer。HUD 使用 UI.Layer.Game，背包 GameMenu，主菜单/设置 Menu，确认框 Modal。
5. 关闭按钮调用页面自身 Close Screen；不要自行 RemoveFromParent 或 AddToViewport。
6. 异步加载使用 Show Screen Async，传 Root 和软类引用。保存 AsyncAction 可取消；取消不发 Completed/Failed。

### 谁打开地图的初始页面？

由地图自己的页面装配入口负责，不再在 Controller 的 BeginPlay 或房间 RPC 中硬编码。第一阶段尚未实现 Experience/GameFeature 页面注入。

可以在关卡蓝图或独立 UI 装配 Actor 中：
- BeginPlay 获取 GameInstance 的 CC_UIManagerSubsystem，绑定 OnRootLayoutReady。
- 绑定后立即用 Get Root Layout For Player 查询已有布局；若非空，执行相同装配函数，避免错过首次通知。
- 装配函数校验 Player 所在 World 是当前地图，检查本地图是否已完成装配，再将主菜单/HUD/大厅页面放入对应 Layer。
- EndPlay 解除事件绑定；切图清理由框架完成。
- 使用原生 CC_PlayerHUDWidget、CC_MainMenuWidget 或自己的派生蓝图作为页面类。

Esc/Start 首次打开暂停菜单的旧硬编码也已删除。玩法输入动作中查询 Root，确认 HasMenu=false 且 IsInputBlocked=false，再 ShowScreen(Menu, 暂停页面类)。菜单内返回仍由 CommonUI 管理。

### 策略配置

DefaultGame.ini 的 /Script/GAS_Demo.CC_UIManagerSubsystem 节配置 DefaultUIPolicyClass。
需要替换统一根布局时派生 CC_UIPolicy，在策略默认值中设置 DefaultRootLayoutClass；没有 Controller 的 RootLayoutClass 覆盖入口。

## 已删除的旧入口

- CC_UIPlayerController 类及其 BeginPlay/EndPlay UI 装配、Esc/Start 硬编码。
- RebuildRoomUI / RebuildRoot，以及 Controller 上的 RootLayoutClass、HUDClass、PauseMenuClass、InitialMenuClass。
- 房间 Controller 的 LobbyScreenClass 和“收到阶段通知就重建 UI”的流程。
- 页面基类与业务 UIController 对特定 PlayerController 子类的转换。

ACC_PlayerController 和 AGAS_DemoPlayerController 直接继承 APlayerController，战斗输入和房间业务保留。房间阶段 RPC 只通知房间子系统连接状态。

旧蓝图若直接继承已删除的 CC_UIPlayerController，必须重新选择合适的 Controller 父类；若使用其 ShowScreen/GetRootLayout/OpenPauseMenu 节点，按上述新入口重新接线。旧默认值不再驱动 UI，没有兼容跳转或静默回退。

## 保留的项目扩展

四层仍由 C++ 构建；根布局仍继承 CommonActivatableWidget，提供最后一页关闭后的 Game 输入回退。Controller/Model、通知、同层同类去重、可配置动画和单机自动暂停仍是项目页面功能，不属于旧 Controller 启动兼容流程，也不是 Lyra 原版功能。

本阶段不包含完整 Experience、GameFeatureAction_AddWidgets、UIExtension、分屏交互模式。不能把这次生命周期调整称为完整复制 Lyra。

## 验证与学习

源码注释说明类、函数和生命周期职责。构建日志在 Saved/UIFrameworkMigration/Build.log，测试日志在 Saved/UIFrameworkMigration/AutomationVerified.log。

本轮删除后编译通过，GASDemo.UI 两项自动化测试通过。BP_CC_PlayerController 仍以 CC_PlayerController 为父类；其资产包含已失效的 InitialMenuClass 序列化字段，源码已不再读取。资产重存清理命令被取消，未修改该蓝图文件。重存前备份位于 Saved/UIFrameworkMigration/RemovedLegacyUIBackup/BP_CC_PlayerController.uasset。

自动化测试包含 Controller 会话隔离、四层隔离、输入令牌、异步取消，以及标准 APlayerController 驱动根布局、临时脱离与永久销毁的区别。实际普通/无缝切图、旧蓝图接线和手柄焦点仍需在编辑器中验收。

建议断点：HandlePlayerAdded → HandleControllerChanged → EnsureRoot；切图观察 DetachWorld → ResetWorldContent → HandleMapLoaded → EnsureRoot，对比切图前后 Root 对象地址。页面容器重新生成 Slate 不等于 Root UObject 被重新创建。

更多页面规则见 [UIFramework.zh-CN.md](UIFramework.zh-CN.md)，业务分层见 [UIArchitecture.zh-CN.md](UIArchitecture.zh-CN.md)。
