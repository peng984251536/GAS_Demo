# UWG_MainMenu 布局脚本

`generate_mainmenu_layout.py` 只生成主菜单布局。完整的 C++ 父类、多人 ListView 和房间条目接入步骤见
[LANMenuWidgetSetup.zh-CN.md](../Docs/LANMenuWidgetSetup.zh-CN.md)。

运行前：

1. `UWG_MainMenu` 的父类设为 `CC_MainMenuWidget`。
2. 在 Designer 中手动放置一个 Overlay 根控件并保存；UE Python 不能设置 WidgetTree 的根控件。
3. 关闭正在使用该资产的编辑页面后，从 Tools → Execute Python Script 运行脚本。

脚本会创建以下五个必需的普通 UMG Button，名称符合 `BindWidget` 契约：

- `Button_SinglePlayer`
- `Button_HostRoom`
- `Button_Multiplayer`
- `Button_LeaveRoom`
- `Button_Quit`

外层尺寸控件使用 `SizeBox_Button_*` 名称，不会占用按钮名。按钮点击由 C++ 自动绑定。
多人页和房间条目请在 Designer 中分别创建；多人页使用 `List_Rooms`（ListView），条目使用行内 `Button_JoinRoom`。
