# UI 控件命名与绑定迁移

布局控件属性使用 `类型_用途`，例如 `TextBlock_Name`、`CommonButtonBase_Refresh`。类型前缀按 C++ 声明去掉 U；数据、Controller、页面所有者引用不属于布局控件绑定。

## 必需绑定

`BindWidget` 要求派生 Widget Blueprint 的设计树提供同名、兼容类型的控件。缺失或类型错误时蓝图编译报错。它不保证所有运行阶段指针非空，仍保留运行时空指针保护。

| 所属类 | 旧控件名 | 新控件名 |
| --- | --- | --- |
| CC_CombatMainWidget | PlayerVitals | PlayerVitalsWidget_PlayerVitals |
| CC_PlayerVitalsWidget | HealthBar | ProgressBar_Health |
| CC_PlayerVitalsWidget | ManaBar | ProgressBar_Mana |
| CC_PlayerVitalsWidget | VitalsText | TextBlock_Vitals |
| CC_MainMenuWidget | Button_SinglePlayer | CommonButtonBase_SinglePlayer |
| CC_MainMenuWidget | Button_HostRoom | CommonButtonBase_HostRoom |
| CC_MainMenuWidget | Button_Multiplayer | CommonButtonBase_Multiplayer |
| CC_MainMenuWidget | Button_LeaveRoom | CommonButtonBase_LeaveRoom |
| CC_MainMenuWidget | Button_Quit | CommonButtonBase_Quit |
| CC_MultiplayerScreenWidget | List_Rooms | ListView_Rooms |
| CC_MultiplayerScreenWidget | Button_Refresh | CommonButtonBase_Refresh |
| CC_MultiplayerScreenWidget | Button_Back | CommonButtonBase_Back |
| CC_MultiplayerRoomItemWidget | Button_JoinRoom | CommonButtonBase_JoinRoom |
| CC_BuffOfferCardWidget | Text_Name | TextBlock_Name |
| CC_BuffOfferCardWidget | Text_Description | TextBlock_Description |
| CC_BuffOfferCardWidget | Image_Icon | Image_Icon（名称不变，改为必需绑定） |
| CC_DamageNumberWidget | DamageText | TextBlock_Damage |

## 可选绑定与原生布局

CC_BuffSelectionScreen 的 `CardContainer` 改为 `PanelWidget_Cards`，类型仍为 UPanelWidget，可用 HorizontalBox 等派生类型。它保留 BindWidgetOptional，因为该页面明确支持在 On Buff Offer Changed 中完全自定义卡片布局、调用 ChooseOption，不使用自动生成容器。

可选绑定的布局控件（不放也能编译；名称或类型不对时不报错，只是不生效）：

| 所属类 | 控件名 | 类型 | 作用 |
| --- | --- | --- | --- |
| CC_ActivatableWidget（所有页面） | CommonButtonBase_Close | CommonButtonBase | 关闭按钮，流程同返回键；bAllowClose 为 false 的页面自动折叠 |
| CC_CombatMainWidget | CommonButtonBase_BuffSelection | CommonButtonBase | 三选一入口，始终可点，无待选奖励时不打开页面 |
| CC_CombatMainWidget | Widget_BuffRedDot | Widget（任意） | 红点，有待选奖励时显示 |
| CC_CombatMainWidget | TextBlock_BuffRewardCount | TextBlock | 待选次数 |

CC_HealthBarWidget、CC_BuffBarWidget、CC_MenuButton、CC_RootLayout、CC_WorldIndicatorLayer 中由 C++ 创建的控件属性也统一了类型前缀。这些 Transient 属性不是 Designer 的绑定要求，无需在蓝图中额外添加同名控件。

## 已有蓝图迁移

1. 关闭编辑器，编译 C++ 后重新打开项目，让反射属性更新。
2. 在各派生蓝图 Designer 中按上表重命名控件；使用 Is Variable，并保留现有布局和样式。
3. 检查 Event Graph 中对旧属性的引用，必要时重新连接；自定义 Default Focus Widget Name 若仍使用旧名称，也需修改。C++ 默认焦点名称已同步。
4. 编译并保存各 Widget Blueprint。缺失或类型错误按编译错误修正。
5. 在游戏里验证主菜单、房间列表、选卡、伤害数字以及战斗 HUD 的显示和交互。

本次源码修改不会自动重命名现有 .uasset 设计树中的控件，也不使用属性重定向绕过布局名称检查。
