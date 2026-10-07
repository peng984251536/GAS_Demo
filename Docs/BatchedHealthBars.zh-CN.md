# 集中绘制头顶血条（C++ / 蓝图接入）

## 已有功能

- 伤害飘字：`UCC_DamageTextSubsystem` 管理列表，`SCC_DamageTextLayer` 集中绘制；已有 `ShowDamageText` 等蓝图入口。
- 玩家 HUD 左上角自身血蓝条：`UCC_PlayerHUDWidget`，可继续保留。

## 接入方式（已由 UI 框架托管）

绘制层现在由 `CC_RootLayout` 的**世界覆盖层**自动创建：位于所有页面层之下、铺满视口、不参与输入，随根布局一起挂载、切图和分屏。打开菜单/弹窗时默认隐藏（根布局的 `Hide World Overlay When Menu Open`），隐藏期间不执行绘制。

1. **不需要**再在 PlayerController 里 Create Widget + Add to Player Screen。若之前这样做过，请删除这些节点，否则血条会画两遍（运行时会输出 Warning 提示）。
2. 想改背景色、边框或屏幕偏移：新建父类为 `CC_BatchedHealthBarWidget` 的蓝图，在根布局蓝图默认值里把 `Health Bar Layer Class` 指向它。设为空则不创建血条层。
3. `CC_EnemyCharacter` 会在 BeginPlay 后的下一帧为每个本地玩家自动注册，配置项在敌人类默认值的 `UI|Health Bar` 分类：`Show Overhead Health Bar` 与 `Overhead Health Bar Options`；EndPlay 时自动移除。
4. 旧的逐角色血条 `CC_WidgetComponent / CC_AttributeWidget` 已删除。曾挂载它们的蓝图在编辑器里会显示组件失效，删除失效组件后重新保存即可。
5. 非敌人角色（Boss、NPC、可破坏物等）仍可手动调用 **Get Health Bar Manager → Register Health Bar**；多人游戏在每个需要显示它的本地客户端注册，注册不通过网络复制。

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

## 数据流

`血条子系统（注册、GAS 订阅、插值）→ OnEntriesUpdated → CC_HealthBarOverlayController（显隐、死亡/满血规则）→ CC_HealthBarOverlayModel → CC_BatchedHealthBarWidget（距离剔除、投影、绘制）`

绘制层只读模型，不访问子系统；控制器由绘制层在构建时创建、释放时销毁。想改显示规则，可派生 `CC_HealthBarOverlayController`，再在血条层蓝图默认值里设置 `Controller Class`。投影换算与伤害飘字共用 `FCC_WorldOverlayProjector`。

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
