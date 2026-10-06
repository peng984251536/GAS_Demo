# 集中绘制头顶血条（C++ / 蓝图接入）

## 已有功能

- 伤害飘字：`UCC_DamageTextSubsystem` 管理列表，`SCC_DamageTextLayer` 集中绘制；已有 `ShowDamageText` 等蓝图入口。
- 原头顶血条：`UCC_WidgetComponent` 在各角色上创建属性控件。
- 玩家 HUD 左上角自身血蓝条：`UCC_PlayerHUDWidget`，可继续保留。

## 新血条的蓝图接入

1. 新建 Widget Blueprint，父类选 `CC_BatchedHealthBarWidget`，例如 `WBP_BatchedHealthBars`。设计器可保持空白；C++ 直接绘制血条。
2. 在本地 PlayerController 创建一次该 Widget，Owning Player 传本地 Controller，调用 **Add to Player Screen**。默认全屏。若嵌入现有 HUD，请设置全屏拉伸且不要套 Retainer Box；可视性保持 Not Hit-Testable。
3. 调用 **Get Health Bar Manager**，传相同本地 PlayerController。存储返回的管理器。
4. 对需要显示血条的角色调用管理器的 **Register Health Bar**：Actor 传该角色，Options 用 **Make CC Health Bar Options** 构造。默认 `Use GAS=true` 自动获取该角色 ASC 的 `CC_AttributeSet.Health / MaxHealth`。
5. 动态生成敌人时同样注册；多人游戏在每个需要显示它的本地客户端注册。不要使用服务器 `GetPlayerController(0)` 代替客户端注册。注册不通过网络复制。
6. 删除敌人蓝图中的旧 `CC_WidgetComponent / BP_CC_WidgetComponent` 头顶血条组件，避免重复显示。现有蓝图资产没有被自动修改；先在一个敌人蓝图上完成接入再批量迁移。

同一个 Actor 重复注册只更新配置，不会产生第二条血条。GAS 延迟创建、属性集延迟到达或 ASC 被替换，会在最多约 0.25 秒后重试/重绑。属性变化通过 GAS 委托更新，不逐帧读取属性。

## 可用接口

| 接口 | 用途 |
| --- | --- |
| Register Health Bar | 注册 / 修改偏移、尺寸、颜色和显示规则 |
| Unregister Health Bar | 移除一个角色并解绑委托 |
| Set Health Bar Visible | 临时隐藏 / 显示 |
| Update Health Bar | 手动推送生命值和最大生命值，需 Use GAS=false |
| Clear Health Bars | 移除所有角色，例如退出战斗 |
| Get Registered Count | 查看数据条目数量，包括暂时隐藏的条目 |

非 GAS 角色：Options 的 `Use GAS=false`，注册后调用一次 `Update Health Bar` 初始化，再在生命变化事件中调用。修改配置后手动模式需要重新推送一次值。

配置项：World Offset（世界厘米）、Size（HUD 单位）、Color、Max Distance（厘米，0=无限）、Hide When Full、Hide When Dead。绘制层可调背景、边框颜色、边框宽度和 Screen Offset。血量插值在 C++ 中进行，首次显示直接取初值。

## 生命周期和性能

每个本地玩家一份管理器和一个绘制层，每个角色只是一条结构体数据；不创建逐角色 Widget、WidgetComponent 或渲染目标。弱引用不延长角色生命周期；销毁、换世界及管理器关闭时清理数据/委托。没有注册条目时管理器停止 Tick。

每帧一次取得投影矩阵，按距离、屏外/背后、Actor 隐藏状态、满血/死亡规则剔除；背景、边框和填充使用同一个白色 Brush、三个固定 Layer，不为每个角色递增层级，为 Slate 合批创造条件。仍有每条可见血条的投影和 DrawElement CPU 成本，实际 draw call 数取决于 Slate 合批、裁剪和 HUD 的其他内容，不能直接宣称只有一个 draw call。

这是屏幕空间头顶 UI，默认会透过墙显示，不做遮挡射线检测。重叠血条按“所有边框 / 所有背景 / 所有填充”的层次绘制，没有逐角色近远遮挡排序。为了保证合批，不给每个角色创建独立材质或文字 Widget。

## 验收

- 编辑器关闭后编译 GAS_DemoEditor；打开编辑器按上述步骤接入。
- 同时注册多个敌人；移动相机、改变窗口位置、DPI 与分辨率，血条应始终跟随角色。
- 伤害、治疗和 MaxHealth 变化更新正确；初始 MaxHealth=0 隐藏，随后有效值到达可显示。
- 同一角色重复注册数量不增加；销毁、手动移除、换地图及 ASC 替换后无残留。
- 联机客户端各自注册；分屏为每个本地 Controller 创建一次 Widget 并独立注册。
- 用 `stat Slate` 与 Slate Insights 比较相同数量敌人的旧组件方案和新方案；GPU 合批数量与帧耗时需要在实际场景测量。

Epic 的 UMG 优化说明：https://dev.epicgames.com/documentation/en-us/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine
