# 头顶血条（可自定义 UMG 控件）

## 结构

| 部分 | 类 | 数量 | 职责 |
| --- | --- | --- | --- |
| 数据 | `CC_BatchedHealthBarSubsystem` | 每个本地玩家 1 个 | 注册、GAS 订阅、血量插值 |
| Controller / Model | `CC_HealthBarOverlayController` / `CC_HealthBarOverlayModel` | 每个血条层 1 个 | 显隐、死亡/满血规则，生成本帧快照 |
| 血条层 | `CC_BatchedHealthBarWidget` | 每个本地玩家 1 个，全屏 | 投影、距离剔除、用对象池摆放单条控件 |
| 单条血条 | 任意实现 `CC Health Bar Item` 接口的 UserWidget | 每个可见敌人 1 个 | 决定一条血条长什么样 |

血条层由 `CC_RootLayout` 的世界覆盖层自动创建，不进页面栈，不参与输入；打开菜单/弹窗时默认隐藏。它和左上角的玩家血蓝条 `CC_PlayerHUDWidget`（Game 层页面）互相独立。

数据流：`子系统 → OnEntriesUpdated → 控制器 → 模型 → 血条层 → 单条控件的 On Health Bar Updated`。

## 自定义一条血条

1. 新建一个普通 UserWidget 蓝图，例如 `WBP_EnemyHealthBar`，按需要设计：进度条、名字、等级、护盾、图标、动画都可以。
2. Class Settings → Interfaces → Add，选 **CC Health Bar Item**，实现三个事件：
   - **On Health Bar Assigned (Target)**：分配给某个敌人时调用。控件会被对象池复用给不同敌人，这里要把上一个敌人留下的文字、动画、颜色全部重置；名字、等级等不常变的信息也在这里读取。
   - **On Health Bar Updated (Data)**：数值变化时调用（插值期间每帧）。`Fraction` 是已平滑的比例，直接给进度条；`Health`/`MaxHealth` 用于显示数字；`Size`/`Color` 是配置里的建议值，可用可不用。
   - **On Health Bar Released**：不再显示、回到对象池时调用，在这里停止动画。
3. 打开角色配置资产（`CC_CharacterConfig`），在 **Character|UI → Overhead Health Bar → Item Widget Class** 里选 `WBP_EnemyHealthBar`。同一份配置的所有敌人都会使用它，不需要改角色蓝图。

布局约定：控件按自身期望尺寸显示（Canvas Auto Size），**底边中点**对齐头顶锚点。不要在控件里自己设置位置；需要上下调整时改配置里的 `World Offset`，或改血条层的 `Screen Offset`。根控件建议用 Size Box 或带固定尺寸的容器，避免尺寸随内容跳动。

没有配置 Item Widget Class 时，使用血条层的 `Default Item Widget Class`，默认是原生的 `CC_DefaultHealthBarItemWidget`（边框 + 背景 + 填充，按配置的 Size/Color 显示）。它也可以派生蓝图后自己做设计树，再覆盖 On Health Bar Updated。

## 角色配置（Character|UI）

| 字段 | 说明 |
| --- | --- |
| Show Overhead Health Bar | 是否显示 |
| Item Widget Class | 单条血条控件，留空用默认控件 |
| World Offset | 锚点 = 角色位置 + 偏移，世界厘米 |
| Size / Color | 建议尺寸（HUD 单位）与颜色，默认控件使用 |
| Max Distance | 超过该距离不显示，0 = 不限 |
| Hide When Full / Hide When Dead | 满血 / 死亡时隐藏 |
| Use GAS | 自动订阅 Health / MaxHealth；关闭后需手动推送数值 |

敌人在 BeginPlay 后的下一帧按配置为每个本地玩家注册，EndPlay 时移除。敌人没有指定角色配置时按默认值显示。

**迁移注意**：敌人类上原来的 `Show Overhead Health Bar` / `Overhead Health Bar Options` 已删除。若之前在角色蓝图里改过这些值，需要在对应的角色配置资产里重新设置一次。

非敌人角色（Boss、NPC、可破坏物等）仍可在蓝图里调用 **Get Health Bar Manager → Register Health Bar** 手动注册，传入同样的 Options。多人游戏在每个需要显示它的本地客户端注册，注册不通过网络复制。

## 血条层可调项

在根布局蓝图默认值的 `Health Bar Layer Class` 指定一个派生自 `CC_BatchedHealthBarWidget` 的蓝图即可调整：

- `Screen Offset`：投影后的屏幕偏移，负 Y 上移。
- `Default Item Widget Class`：未配置时使用的单条控件。
- 设计树：可以放一个名为 `ItemCanvas` 的 Canvas Panel 作为单条控件的容器（外面可以再包其他控件做整体效果）；设计树为空时自动创建。

## 子系统接口

| 接口 | 用途 |
| --- | --- |
| Register Health Bar | 注册 / 修改配置（包括单条控件类） |
| Unregister Health Bar | 移除一个角色并解绑委托 |
| Set Health Bar Visible | 临时隐藏 / 显示 |
| Update Health Bar | 手动推送生命值和最大生命值，需 Use GAS=false |
| Clear Health Bars | 移除所有角色，例如退出战斗 |
| Get Registered Count | 数据条目数量，包括暂时隐藏的条目 |

## 性能

每个**可见**血条一个 UMG 控件，不可见的回到对象池（折叠、不绘制），对象池按控件类分组，数量等于同时可见的最大数量。每帧做一次投影矩阵计算，每个可见血条更新一次 Canvas 位置；数据没变时不调用蓝图事件。

同屏几十个敌人没有问题。上百个同时可见时，开销会明显高于旧的批量绘制方案；那时可以简化单条控件（少用嵌套、避免每帧改文字），或缩短 Max Distance。用 `stat Slate` 与 Slate Insights 在实际场景测量。

这是屏幕空间头顶 UI，默认透墙显示，不做遮挡检测；重叠时不按远近排序。

## 验收

- 编译 GAS_DemoEditor（新增接口和类型头文件，需要重新生成项目文件）。
- 不做任何配置：敌人显示默认血条，外观与之前一致。
- 新建 WBP 实现接口并配到角色配置：该配置的敌人都换成新外观；改配置无需改角色蓝图。
- 敌人死亡、离开画面、超出距离后再出现：控件被复用，没有残留上一个敌人的文字或动画。
- 伤害、治疗、MaxHealth 变化时数值正确；打开菜单时血条隐藏，关闭后恢复。
- 移动相机、改窗口位置、DPI、分辨率、分屏，血条跟随角色。
