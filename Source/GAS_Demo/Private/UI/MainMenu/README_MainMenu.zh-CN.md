# 主菜单接入

当前接入方法见 [LANMenuWidgetSetup.zh-CN.md](../../../../../Docs/LANMenuWidgetSetup.zh-CN.md)。

职责边界：

- `CC_MainMenuWidget`：单人、LAN 建房、打开房间浏览页、离房清理和退出。
- `CC_MultiplayerScreenWidget`：刷新 LAN 房间列表并进入房间。
- `CC_MultiplayerRoomItemWidget`：展示一条房间信息，行内 `Button_JoinRoom` 加入该房间。

所有必需控件都使用 `BindWidget`。多人页使用 `List_Rooms`（ListView），不再使用统一加入、建房或离房按钮。
