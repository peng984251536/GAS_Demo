# 主菜单和局域网页面接入

这两个类是 Widget Blueprint 的 C++ 父类，布局与样式由 Designer 制作。
它们继承 CommonActivatableWidget，仍属于 UserWidget；请通过项目 CommonUI 栈打开。
单独 Create Widget + Add to Viewport 没有 RootLayout，控制器会拒绝操作。

## 页面与入口

1. 创建 `WBP_MainMenu`，父类选择 `CC_MainMenuWidget`。
2. 创建 `WBP_Multiplayer`，父类选择 `CC_MultiplayerScreenWidget`。
3. 创建 `WBP_RoomItem`，父类选择 `CC_MultiplayerRoomItemWidget`。
4. 前端使用任意本地 PlayerController。地图 UI 装配入口调用 `Get Root Layout For Player`，
   在 Root 上调用 `ShowScreen(UI.Layer.Menu, WBP_MainMenu)`。
   根布局尚未就绪时订阅 UIManager.OnRootLayoutReady；具体接线见 LyraUI-Phase1.zh-CN.md。
5. 保留两个页面默认的 `Controller Class` 即可使用单人和 LAN 功能。
   设置页在 `CC_MainMenuController` 子蓝图的 `Settings Screen Class` 配置，
   再将页面的 Controller Class 改为该子蓝图。退出确认框已有原生默认值。
6. 主菜单默认禁止返回和关闭；多人页允许返回。返回只退页面，不取消后台 Session 操作。

## Designer 按钮自动绑定

下面控件全部使用 `BindWidget`，名称和类型是页面契约。按钮必须是普通 UMG `Button`，
不能把名称给外层 SizeBox/Border；缺失或类型错误时 Widget Blueprint 会直接编译失败。
按钮点击已由 C++ 绑定，不需要再创建蓝图 OnClicked。

| 页面 | 控件名 | 操作 |
|---|---|---|
| 主菜单 | `Button_SinglePlayer` | `StartSinglePlayer` |
| 主菜单 | `Button_HostRoom` | `HostLANRoom` |
| 主菜单 | `Button_Multiplayer` | `SearchRooms`，打开多人页并搜索 |
| 主菜单 | `Button_LeaveRoom` | `LeaveCurrentRoom` |
| 主菜单 | `Button_Quit` | `RequestQuit`，打开确认框 |
| 多人页 | `List_Rooms`（ListView） | 显示并滚动当前搜索结果 |
| 多人页 | `Button_Refresh` | `RefreshRooms` |
| 多人页 | `Button_Back` | `GoBack` |
| 房间条目 | `Button_JoinRoom` | 加入该条目代表的房间 |

代码自动更新忙碌/可加入/可恢复状态对应的按钮禁用状态，并在释放时解绑。
默认焦点是主菜单 Button_SinglePlayer、多人页 Button_Refresh；可修改 Default Focus Widget Name。

## Class Defaults

主菜单配置 Single Player Map、Room List Screen Class=WBP_Multiplayer、Search Request、Host Request、Front End Map。
多人页只配置 Search Request；建房和离房属于主菜单。

- Single Player Map 是单人玩法地图；当前入口只开地图，不加载存档。
- Host Request 设置 DisplayName、LobbyMap、BuildId、MaxPlayers 等。
- 主菜单 Host Request 与多人页 Search Request 的 BuildId 保持一致（默认 dev）。
- Front End Map 是离房/恢复清理后返回的主菜单地图。
- 页面强制使用 LAN 搜索/建房；项目已有 OnlineSubsystemNull 配置。
- LobbyMap 使用 CC_RoomLobbyMode 或 CC_RoomGameMode 派生类，并保留 RoomPlayerController，服务器才能确认进房。
- 直接进入可游玩的世界：使用 CC_RoomGameMode 子蓝图并配置 Pawn。
- 先准备再开始：使用 CC_RoomLobbyMode 并配置 GameMap，大厅地图入口显式装配大厅页面。
- 打包时将上述地图加入打包地图列表。

## 状态与列表

主菜单实现 `On Main Menu State Changed`，使用 State.ActionError、State.LastRoomResult.Message、State.bBusy。
也可主动调用 GetMainMenuState。

在 `WBP_Multiplayer` 中放置 `ListView` 并命名为 `List_Rooms`，将它的
`Entry Widget Class` 设置为 `WBP_RoomItem`。ListView 自己负责滚动与行复用，页面 C++ 负责填充数据。

多人页实现 `On Multiplayer State Changed(State)`，用于更新加载动画、空列表提示和错误文字。
房间条目实现 `On Room Item Changed(Room, bCanJoin)`：

1. 用 Room 显示 DisplayName、HostName、CurrentPlayers/MaxPlayers、PingMs。
2. 用 bCanJoin 表现按钮禁用样式；实际启用状态已经由 C++ 设置。
3. 行内 `Button_JoinRoom` 自动加入本行房间，无需保存索引或手动传 RoomId。
4. 多人页忙碌时显示等待；空列表显示“未发现局域网世界”。
5. 错误优先使用非空 ActionError，否则使用 LastRoomResult.Message。

房间行视觉布局由项目提供。搜索开始即清空 ListView，RoomId 不能跨搜索缓存。
操作返回 true 只代表已发出请求，远端连接最终结果由状态事件通知。
搜索失败仍保留多人页，允许显示错误并重试。若进入 RecoveryRequired，返回主菜单点击离房清理。

## 验证与范围

两台同一局域网设备运行同一构建：房主创建，另一台刷新并加入。
单机推荐两个独立进程；确认防火墙允许 LAN 通信。
验证空结果、重复刷新、加入、房主退出、失败后的清理。
返回只退页面，离开才清理 Session；房主离开会结束房间。

当前实现“菜单创建 listen-server 世界 + 搜索加入”。
不包含存档/世界列表，也不包含单人游戏中无切图开放 LAN。
现有 RequestStartGame/ExecuteStartGame/CompleteStartGame 可继续用于自定义存档控制器。
