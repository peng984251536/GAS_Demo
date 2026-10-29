# 世界指示器（头顶血条、伤害数字）

所有"跟随世界位置的屏幕 UI"走同一套指示器系统，参考 Lyra Indicator System 精简实现。血条和伤害数字只是两个数据源，共用一个显示层。

## 结构

| 部分 | 类 | 数量 | 职责 |
| --- | --- | --- | --- |
| 数据 | `UCC_WorldIndicator` | 每个指示器 1 个 | 锚点（Actor 或固定位置）、世界/屏幕偏移、对齐、最大距离、优先级、控件类、是否可见 |
| 注册表 | `UCC_WorldIndicatorSubsystem` | 每个本地玩家 1 个 | 增删指示器，只广播"新增/移除"，不 Tick |
| 显示层 | `UCC_WorldIndicatorLayer` | 每个本地玩家 1 个 | 全屏 Canvas；控件对象池、软类异步加载；每帧投影一次，只改 RenderTranslation |
| 控件接口 | `ICC_WorldIndicatorWidget` | — | `BindIndicator` / `UnbindIndicator`，控件自己读数据、订阅 GAS |
| 血条数据源 | `UCC_HealthBarSource` | 每个本地玩家 1 个 | 角色 ASC 就绪时创建跟随指示器，角色 EndPlay 时移除 |
| 数字数据源 | `UCC_DamageNumberSource` | 每个本地玩家 1 个 | 接收伤害 Cue 的数字，做容量限制、同目标错位、指示器对象复用 |

显示层由 `CC_RootLayout` 创建在 WorldOverlay：位于所有页面层之下、SafeZone 之外，不进页面栈，打开菜单时隐藏。不要再把血条或飘字控件手动放进 HUD。

角色发现走静态事件 `ACC_BaseCharacter::OnAnyCharacterReady`（`BroadcastASCInitialized()` 里发出），玩法代码不依赖任何 UI 类。

## 每帧开销

- 数据源与注册表不 Tick。
- 显示层每帧：一次视图矩阵计算 + 每个可见指示器一次投影和距离剔除 + `SetRenderTranslation`。显隐只在状态变化时切换，不触发重新布局。
- 血条控件只在插值未结束时更新宽度；属性变化来自 ASC 委托。
- 伤害数字默认由代码做上浮和淡出（`bDriveAnimationInCode`），寿命结束后回到池中。
- 合批取决于控件使用的材质、贴图/图集、字体和裁剪，与控件类是否相同无关。所有血条用同一图集、所有数字用同一字体时批次最少。

## 配置

**项目设置 → Game → CC World Indicator**

| 字段 | 默认 | 说明 |
| --- | --- | --- |
| Default Health Bar Widget Class | `CC_HealthBarWidget` | 角色配置没指定血条控件时使用 |
| Damage Number Widget Class | `CC_DamageNumberWidget` | 伤害/治疗数字控件 |
| Max Damage Numbers Per Player | 64 | 超过时先淘汰最旧的普通数字；全是暴击/治疗时丢弃新的普通数字 |
| Show All Damage Numbers | false | false 只显示本地玩家造成或受到的数字 |

**角色配置 `UCC_CharacterConfig`**

| 字段 | 说明 |
| --- | --- |
| `bShowOverheadHealthBar` | 是否显示头顶血条；本地玩家自己的角色始终不显示 |
| `OverheadHealthBar.WidgetClass` | 这个角色用的血条控件，留空用项目默认 |
| `OverheadHealthBar.WorldOffset` | 相对角色位置的世界偏移，默认 (0,0,80) |
| `OverheadHealthBar.MaxDistance` | 超过这个距离不显示，≤0 表示不限 |

## 自定义控件

**血条**：在编辑器里派生 `CC_HealthBarWidget`（或任意实现 CC World Indicator Widget 接口的 UserWidget）。

- 设计器里摆了控件时，原生默认外观不生成；在 `On Health Bar Updated (Health, MaxHealth, Fraction)` 里驱动自己的进度条，`Fraction` 是插值后的显示比例。
- `On Health Bar Bound (Target)` 在绑定到角色时调用，可读取角色信息（名字、等级等）。
- 可调：`bHideWhenFull`、`bHideWhenDead`、`InterpSpeed`；默认外观的 `BarSize`、颜色、边框。
- 把蓝图填到项目设置或某个角色配置的 `OverheadHealthBar.WidgetClass`。

**伤害数字**：派生 `CC_DamageNumberWidget`。

- 必须放一个名为 `TextBlock_Damage` 的 TextBlock 会自动填入数字和样式（字体、字号、颜色按 Normal / Critical / Heal 区分）。
- `On Damage Number Shown (Amount, Style, DamageType)` 可按伤害类型换颜色或播放 UMG 动画。
- 要用 UMG 动画代替代码动画：关掉 `bDriveAnimationInCode`，动画结束时调用 `Finish Damage Number`。

**新的世界 UI**（任务标记、交互提示、名字牌）：写一个控件实现接口，再在任何地方创建 `UCC_WorldIndicator` 填好锚点和控件类，调用 `UCC_WorldIndicatorSubsystem::GetForPlayer(PC)->AddIndicator()`；不用时 `RemoveIndicator()`。需要附加数据时派生指示器类（参考 `UCC_DamageNumberIndicator`）。显示层不需要改。

## 数据流

```text
血条：角色 ASC 就绪 → OnAnyCharacterReady → HealthBarSource 创建指示器 → Subsystem.OnIndicatorAdded
      → Layer 从池取控件、BindIndicator → 控件订阅 Health/MaxHealth 委托
      角色 EndPlay → HealthBarSource 移除 → Layer UnbindIndicator、回池

数字：伤害 GE 的 GameplayCue.Combat.DamageText → CC_GameplayCue_DamageText 按本地玩家筛选
      → DamageNumberSource.ShowNumber → AddIndicator → Layer 取控件 → 动画结束 FinishDamageNumber → RemoveIndicator → 指示器回池
```

伤害数字的数值与触发规则见 [DamageSystem.zh-CN.md](../Gameplay/DamageSystem.zh-CN.md#飘字)。

## 从旧实现迁移

- 已删除：`CC_BatchedHealthBarSubsystem/Widget`、`CC_HealthBarOverlayController/Model`、`CC_HealthBarItem`、`CC_HealthBarTypes`、`CC_DefaultHealthBarItemWidget`、`CC_WorldOverlayController`、`CC_DamageTextSubsystem`、`CC_DamageTextWidget`、`SCC_DamageTextLayer`、`CC_DamageTextEntryWidget`。引用它们的蓝图需要改父类或删除节点。
- 根布局的 `Health Bar Layer Class` / `Damage Text Layer Class` 改为一个 `World Indicator Layer Class`。
- 角色配置的 `OverheadHealthBar` 类型变了，原来填的值会重置，需要重新填。
- 旧的"显示全部伤害"开关移到项目设置 `Show All Damage Numbers`。

## 测试

- `GAS_Demo.UI.WorldIndicator.SubsystemAndSources`：注册表增删、血条源去重与随角色销毁移除、数字容量限制。
- `GAS_Demo.Damage.CueAndContext`：Cue → 数字源的数量、数值、位置、错位、免疫、治疗样式和回收。
