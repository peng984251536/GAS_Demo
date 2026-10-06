# UI 框架

入口：CC_UIManagerSubsystem → CC_UIPolicy → LocalPlayer 的 CC_RootLayout。直接监听引擎玩家与地图事件，不依赖项目 PlayerController。

旧 CC_UIPlayerController / RebuildRoomUI 已删除。切图清空页面并重新挂载相同 Root；玩家退出才永久释放。页面经 Get Root Layout For Player 查询并导航。

完整接入与旧蓝图迁移步骤见 Docs/LyraUI-Phase1.zh-CN.md；页面规则见 Docs/UIFramework.zh-CN.md。
