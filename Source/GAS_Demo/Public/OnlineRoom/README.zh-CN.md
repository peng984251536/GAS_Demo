# OnlineRoom：创建、查找、进入房间与大厅

本目录提供 **Listen Server + 经典 OnlineSubsystem Session + 联机大厅** 的 C++ 功能。
对外头文件集中在 `Public/OnlineRoom`，实现在 `Private/OnlineRoom`，回归测试在
`Private/OnlineRoom/Tests`。沿用 UE 的 Public/Private 目录约定，不额外拆一个构建模块。

当前默认后端为 OnlineSubsystemNull，适用于本机独立进程/局域网。代码已提供蓝图调用接口，
尚未创建或修改地图、GameMode 蓝图和 Widget 资产。按下面步骤配置后才能在界面操作。

## 职责与设计模式

| 类型 | 职责 | 设计思路 |
| --- | --- | --- |
| `UCC_OnlineRoomSubsystem` | 蓝图入口、异步状态、Travel、失败缓存 | Facade + 显式状态机 |
| `ICC_RoomSessionBackend` | 定义建房、搜索、加入、退出契约 | Strategy / 依赖倒置 |
| `FCC_OnlineSubsystemRoomBackend` | SessionSettings、OSS 回调、原始搜索结果 | Adapter + RAII 解绑 |
| `FCC_RoomSummary` 等结构 | UI 输入、搜索快照、稳定错误码 | DTO，UI 不引用 OSS 类型 |
| `ACC_RoomGameMode` | 准入、容量、到达流程、返回大厅 | Template Method |
| `ACC_RoomLobbyMode` | 服务器准备/开始规则、目标地图 | 权威规则集中；额外规则为扩展点 |
| `ACC_RoomLobbyState` | 房间阶段、容量、成员列表 | 复制模型 + Observer |
| `ACC_RoomPlayerState` | 成员准备与房主状态，继承现有 GAS PS | 扩展既有玩家模型 |
| `ACC_RoomPlayerController` | 带连接所有权的 RPC 和本地界面切换 | Command 入口，规则转交 GameMode |

模式用于隔离变化源，不需要给每个状态各建一个 UObject。成员数量较小时，原生 PlayerArray
与 RepNotify 已足够；暂不增加 FastArray、独立消息总线或房间专属 MVVM 框架。

主菜单只依赖 Subsystem，房间列表只消费 DTO。大厅只读 GameState / PlayerState，
准备/开始通过自己拥有的 PlayerController 发给服务器。GameInstanceSubsystem **不复制**，
因此客户端上的 HostRequest 不能用于权限判断。

## 蓝图与地图接入

1. 创建空的 `FrontEndMap` 和 `LobbyMap`，保留现有战斗地图。
2. 新建 `BP_RoomPlayerState`，父类选择 `CC_RoomPlayerState`，复制原玩家 PS 的 `CharacterConfig` 默认值。
3. 新建 `BP_RoomPlayerController`，父类选择 `CC_RoomPlayerController`，复制原控制器的
   MappingContexts、InputConfig、输入标签。UI 类配置不再放在 Controller 上。
   大厅页面由地图 UI 装配入口通过 UIManager 查询 Root 后打开。
4. 新建 `BP_RoomLobbyMode`，父类选择 `CC_RoomLobbyMode`。
   Controller / PlayerState 设为上面两个蓝图；保留原生 `CC_RoomLobbyState`；DefaultPawnClass 留空。
   在 `GameMap` 中选择战斗地图，`MinimumPlayers` 是包括房主在内的最少人数。
5. 新建 `BP_RoomGameMode`，父类选择 `CC_RoomGameMode`。
   Controller / PlayerState 使用同一对房间蓝图；DefaultPawnClass 使用项目现有玩家角色蓝图。
   项目原本 GameMode 蓝图中的战斗业务需要转接到新模式；旧资产并未自动迁移。
6. LobbyMap 的 World Settings → GameMode Override 指定 BP_RoomLobbyMode；战斗地图指定 BP_RoomGameMode。
7. FrontEndMap 使用独立的前端 GameMode，无战斗 Pawn；本地控制器可继承 APlayerController。
   前端地图显式装配主菜单，接入步骤见 Docs/LyraUI-Phase1.zh-CN.md。
8. 项目 Game Default Map 设为 FrontEndMap。可指定一张很小的 Transition Map；房间 GameMode 已启用
   bUseSeamlessTravel。打包时确保前端、大厅、战斗、过渡地图均被 Cook。

当前没有运行时选角色。先通过同一 PlayerState 蓝图的 CharacterConfig 配置固定角色。
以后增加选人时，建议用服务器白名单解析 CharacterId，并实现配置复制与 ASC 就绪链路；
不能只改客户端 CharacterConfig 指针。

## 主菜单节点调用

通过 `Get Game Instance Subsystem (CC_OnlineRoomSubsystem)` 获取入口。无需自定义 GameInstance 类。

| 按钮/页面 | 调用 |
| --- | --- |
| 创建 | Make FCC_RoomCreateRequest → HostRoom；LobbyMap 必填，BuildId 非空，MaxPlayers 1–64（含房主） |
| 刷新 | Make FCC_RoomSearchRequest → FindRooms；Null 测试设置 IsLANQuery=true |
| 进入 | 最新 FCC_RoomSummary 的 RoomId → JoinRoom |
| 离房/解散 | LeaveRoom(FrontEndMap)；客机退出自身，房主结束监听服务 |
| 进度遮罩 | 订阅 OnStateChanged，根据 GetState / IsBusy 更新 |
| 搜索列表 | 订阅 OnSearchCompleted，并在页面激活时先读 GetCachedRooms |
| 错误弹窗 | 订阅 OnOperationCompleted；页面重建后读 GetLastResult |

bool 返回值表示请求是否受理，**不代表房间已经连接成功**。Host/Join 最终成功由服务器的
CC_RoomPlayerController 到达确认后触发。只有加载地图、甚至 JoinSession 成功，都不足以确认登录完成。
Travel 等待超过 60 秒会报告失败；这也能发现目标地图没有配置房间 GameMode 的情况。

RoomId 是本轮搜索的临时令牌，不是可分享的房间码。刷新后旧令牌失效，旧列表项应一起清掉。
BuildId 与项目标记既传给后端，也在收到结果后本地过滤，因为 LAN 后端不保证应用所有搜索条件。
列表的 Joinable 是搜索时的空位快照，最终是否允许进入由服务器决定。

CommonUI 页面在激活时绑定事件并主动读一次快照，失活时解绑；不要把 Widget 保存在 Subsystem 中。
事件广播期间禁止同步发起新操作；要在回调中自动重试，请排到下一帧。

## 大厅节点调用

- 页面激活：Get Game State → Cast CC_RoomLobbyState → GetMembers；订阅 OnLobbyChanged。
- 每行：读取 PlayerName、IsReady、IsRoomHost。名字显示仍使用 UE 原生 PlayerState 字段。
- 准备按钮：Get Owning Player → Cast CC_RoomPlayerController → ServerSetReady。
- 开始按钮：同一控制器调用 ServerStartRoomGame。默认房主不需要额外准备，所有客机必须准备。
- 开始失败：在控制器的 OnRoomCommandFailed 蓝图事件显示 Reason。
- 额外限制：在 LobbyMode 蓝图重写 ValidateAdditionalStartRules，返回 false 和说明即可。
  原生房主、阶段、人数、准备、地图存在性校验仍先执行。
- 战斗结算：仅服务器取得 CC_RoomGameMode → ReturnToLobby；客户端不自行 OpenLevel。

PlayerArray 到达和各 PlayerState 属性复制可能不在同一帧。OnLobbyChanged 会在成员增删、准备/房主
状态到达时触发；页面激活时仍必须先读取一次。回大厅时重新清空准备状态。

UIManager 监听引擎玩家与地图事件。无缝切图时清空页面并重新挂载同一根布局，不依赖 Controller
重复 BeginPlay。房间阶段通知仅更新房间业务，不再打开页面或重建 UI。

## 错误与恢复

重复点击或参数无效只会被拒绝，不会重置正在进行的建房/搜索事务。
平台会话尚存而 Travel/Join 失败时状态为 `RecoveryRequired`；显示原因，然后调用
`LeaveRoom(FrontEndMap)` 清理，再让用户重新加入或创建。底层 DestroySession 失败时保留恢复状态，
允许再次退出。不要直接跳过 Session 清理去调用 JoinRoom。

房主退出后不迁移主机；客户端由引擎连接失败流程离开连接，前端页从 GetLastResult 展示原因，
并通过 LeaveRoom 清理本地 Session。务必将默认地图配置成前端地图。
监听服务器上单个客机的断线不会把整个房主会话标记为失败。

平台 Create/Find/Join/Destroy 的超时由对应 OSS 实现负责，本层不会假装取消一个后端仍在运行的请求。
目前未提供取消搜索、自动重连、自动重试、房间密码、邀请、主机迁移或独立服务器分配。
Travel 超时后需要离房清理，不建议直接重试原请求。

Session 当前用于发现与连接，未实现 StartSession/EndSession/UpdateSession 驱动的在线比赛状态。
因此搜索列表可能暂时显示正在切图或禁止中途加入的房间，服务器准入会拒绝它们。以后增加
后端状态更新时，可在 Adapter 扩展一个 UpdateRoom 接口，不需改大厅 Widget。

## 扩展后端

经典 OSS Steam/EOS 可以复用当前适配器，但需要另行完成账号登录、插件、NetDriver/P2P、平台后台与
会话标志配置；把 Null 改成 Steam/EOS 并不自动完成互联网接入。这里未接入平台账号或上线配置。

如果换成另一套 API，可实现 ICC_RoomSessionBackend，并通过 C++ 派生门面的 CreateBackend 工厂提供。
当前 ShouldCreateSubsystem 在存在派生类时让基类退让；同一进程只保留一个具体门面子类，避免多个
叶子子类争用 GameSession。实现契约：每个请求无论异步完成还是同步拒绝，都要在游戏线程回调一次；
Shutdown 后不得再回调；平台原始搜索结果由适配器持有。

## 对现有代码的接点

- Build.cs 增加私有 OnlineSubsystem / OnlineSubsystemUtils 依赖。
- uproject 启用 OnlineSubsystemNull；DefaultEngine.ini 默认使用 Null。
- UI 生命周期由 UIManager/UIPolicy 管理，旧 Controller 重建入口已删除。
- CC_BaseCharacter 初始能力授予前检查同类 Spec，防止重生/复用 ASC 时重复授予。
  如果将来确实需要同一能力类的多个来源，应把这个简单去重升级为带来源与句柄的 AbilitySet 管理。
- 当前 CharacterConfig、战斗预测、伤害判定和运行时选人机制没有在本任务中改造。

## 验证记录与最短联调

已完成一轮 UnrealHeaderTool 反射生成检查。当时新头文件通过，生成 25 个文件。
后续增加普通 C++ 回归测试和生命周期防护；未再次编译。完整构建在可用内存仅约 320 MB 时排队
84 个串行步骤，按用户要求停止，未完成 C++ 编译/链接；测试尚未执行，尚未进行双进程运行验证。

已提供自动化测试 `GAS_Demo.OnlineRoom.State.RejectionAndRecovery`，覆盖重复请求不会清空活动事务、
失败残留 Session 强制先清理、事件广播期间不可重入。该测试使用内存后端，不连接在线服务。

配置好资产并完成编译后，优先在两个独立游戏进程或两台同局域网电脑验证：

1. 双方从 FrontEndMap 启动；房主建房，客机刷新并加入。
2. 客机准备，双方都看到变化；客机不能直接开始，房主可开始。
3. 双方进入战斗，能控制各自角色；服务器返回大厅，准备清空，页面不重复。
4. 客机离房再加入；房主解散；加入满房/已消失房间；失败清理后重新建房。
5. 连点创建/刷新、使用刷新前的旧 RoomId、不同 BuildId，确认状态不会错乱。

本机 PIE 自动作为 Client 启动会绕过房间流程；优先使用 Standalone 独立进程。
跨机器 LAN 还取决于防火墙和局域网广播可达性，Null 不提供互联网 NAT 穿透。
战斗预测与命中逻辑仍需单独做延迟/丢包验证。
