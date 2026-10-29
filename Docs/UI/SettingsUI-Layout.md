# 设置界面 UI 布局方案

> 约束条件：PC 键鼠为主 · 当前只做 Gameplay 分类 · CommonUI + 蓝图 · 主菜单与暂停菜单共用同一套

---

## 一、先定三件事，再画布局

布局怎么分，本质取决于三个前提。这三个定错了，后面控件摆得再整齐也要返工。

**1. 用普通 UserWidget 还是 ActivatableWidget？**

CommonUI 官方设计指南给了一条判断标准：只有当这个控件**需要影响输入路由**（开关状态决定输入给谁）、**有多个可交互子控件**、或者**需要挡住背后 UI 的输入**时，才用 `CommonActivatableWidget`。否则用 `CommonUserWidget` 就够，用重了反而会带来焦点和层级上的副作用。

对应到设置界面：

| 控件 | 应该继承 |
|---|---|
| 设置屏幕本体 | `CommonActivatableWidget`（要被推入栈、要接管输入） |
| 每个分类页（如 Gameplay 页） | `CommonActivatableWidget`（页间切换是焦点作用域切换） |
| 一行设置（标签 + 滑块） | `CommonUserWidget`（纯展示容器，不需要输入路由） |
| 侧边导航的一个条目 | `CommonButtonBase` 派生 |
| 弹窗（"确定要恢复默认吗"） | `CommonActivatableWidget`（必须挡输入） |

**2. 页面切换用什么容器？**

用 `CommonActivatableWidgetSwitcher`，**不要**用普通 `WidgetSwitcher`。前者会把非当前页真正地"失活"，输入自然只落在当前页；后者只是切可见性，隐藏页的控件仍然参与焦点系统，会出现焦点跑到看不见的按钮上的问题。

这里有个容易混淆的点：继承链是 `WidgetSwitcher` → `CommonAnimatedSwitcher` → `CommonActivatableWidgetSwitcher`。中间那个 `CommonAnimatedSwitcher` 名字里带 "Animated"，但它**只负责过渡动画，并不会激活/失活子控件**——它的类注释写了会激活，但那是文档错误。真正做激活的是继承链末端的 `CommonActivatableWidgetSwitcher`。所以别停在中间那一层。

如果你不需要过渡动画、只想要"切换时正确激活"，还有个更轻的选择：`Common Visibility Switcher`（继承自 Overlay），行为跟普通 `WidgetSwitcher` 一样但会在子控件可见时激活它们。

时序上要注意：`On Deactivated` 在切换**瞬间**就触发（旧页在淡出时已经是失活状态），而 `On Activated` 要等旧页**完全移出视野**才触发（新页在淡入时已经是激活状态）。如果你在 `On Activated` 里写依赖布局的代码，要考虑到此时动画还在跑。

**3. 绝对不要用 Canvas Panel 摆元素**

官方 DPI 缩放文档和 CommonUI 设计指南都提到这一点。Canvas Panel 是像素绝对定位，在 1920×1080 下摆好的东西换到 1280×720 或超宽屏会直接错位或跑出屏幕。

正确做法是：**Canvas Panel 只在最外层当一次容器用**，用来把内容居中或贴边；内部所有布局一律用 `VerticalBox` / `HorizontalBox` / `GridPanel` / `SizeBox`。

---

## 二、整体分区：三区结构

设置界面是典型的"三区"布局。这个结构几乎是通用解，因为底部操作栏和顶部关闭按钮必须在任何分辨率下都不随内容滚动。

```
┌─────────────────────────────────────────────────────┐
│  Header                            设置       [✕]   │  固定高 64~72
├──────────────┬──────────────────────────────────────┤
│              │                                      │
│   侧边导航    │        当前分类的设置内容              │  Fill，内容区可滚动
│              │        （ScrollBox）                  │
│  · 玩法       │                                      │
│  · (画面)     │                                      │
│  · (声音)     │                                      │
│              │                                      │
├──────────────┴──────────────────────────────────────┤
│  [恢复默认]                        [取消]   [应用]    │  固定高 72~80
└─────────────────────────────────────────────────────┘
```

**为什么保留侧边导航，即使现在只有一个分类？**

因为你选了"以后可扩展"的路线。现在只放"玩法"一项，加"画面"时是纯数据操作（数据表加一行 + 加一个页面 widget），不用动布局。反过来，如果现在做成单页无导航，以后加分类就得重构整个屏幕。多花的是一个 220px 的 SizeBox，值。

如果你确定永远只有一个分类，可以去掉侧边栏，把内容区直接占满——但我不建议赌这个。

---

## 三、控件树

这是可以直接照着搭的结构。缩进表示父子层级。

```
W_SettingsScreen  (: CommonActivatableWidget)
└─ Overlay                                   ← 最外层，用于叠加弹窗层
   ├─ SizeBox            MaxWidth=1100, MaxHeight=720
   │  └─ Border          背景色 + 圆角 + 内边距 32
   │     └─ VerticalBox
   │        ├─ HorizontalBox                    [Header, 高 64]
   │        │  ├─ TextBlock        "设置"       字号大，左对齐
   │        │  ├─ Spacer           Fill
   │        │  └─ W_IconButton     "✕"          关闭
   │        │
   │        ├─ HorizontalBox                    [Body, Fill]
   │        │  ├─ W_SettingsNav     SizeBox MinWidth=240
   │        │  │  └─ VerticalBox
   │        │  │     └─ W_NavEntry × N         "玩法" / "画面" / ...
   │        │  ├─ Border           宽 1，作竖直分割线
   │        │  └─ CommonActivatableWidgetSwitcher      ← 关键：不是 WidgetSwitcher
   │        │     └─ W_GameplayPage (: CommonActivatableWidget)
   │        │
   │        └─ HorizontalBox                    [Footer, 高 72]
   │           ├─ W_CommonButton    "恢复默认"
   │           ├─ Spacer            Fill
   │           ├─ W_CommonButton    "取消"
   │           └─ W_CommonButton    "应用"
   │
   └─ W_ConfirmDialog               默认 Collapsed，恢复默认时显示
```

分类页内部：

```
W_GameplayPage  (: CommonActivatableWidget)
└─ ScrollBox
   └─ VerticalBox
      ├─ W_SettingsSection   "难度"
      │  └─ VerticalBox
      │     ├─ W_SettingsRow   难度       [下拉]
      │     ├─ W_SettingsRow   敌人攻击性  [滑块]
      │     └─ W_SettingsRow   自动存档    [开关]
      ├─ W_SettingsSection   "界面"
      │  └─ VerticalBox
      │     ├─ W_SettingsRow   语言        [下拉]
      │     └─ W_SettingsRow   字幕        [开关]
      └─ ...
```

### 复用原子：W_SettingsRow

整个界面能保持整齐，靠的是这一行控件的设计。它把所有设置行的对齐规则收进一个地方，改一次全局生效。

```
W_SettingsRow  (: CommonUserWidget)
└─ HorizontalBox                 高 56，垂直居中
   ├─ SizeBox       Width=280     标签列，固定宽
   │  └─ TextBlock  设置名         左对齐
   ├─ Spacer        Fill=8
   └─ SizeBox       MinWidth=320  控件列，固定宽
      └─ [Named Slot]              ← 外部往里塞下拉/滑块/开关
```

**为什么标签列和控件列都要固定宽？** 因为如果让内容自适应，每一行的控件起始位置会随着标签文字长短而漂移，整个页面看起来是歪的。固定宽度牺牲了一点紧凑性，换来的是纵向对齐。这是设置界面里最容易被忽略、但一眼就能看出来的细节。

用 Named Slot 而不是硬编码控件类型，是为了让同一行能装下拉、滑块、开关三种东西。

---

## 四、尺寸参数（设计分辨率 1920×1080）

先在 `Project Settings → User Interface → DPI Scale Rule` 设为 **Shortest Side**（最常用，能保证不同宽高比下 UI 不被裁切）。下面的数值都基于 1920×1080 设计稿。

| 项目 | 建议值 | 说明 |
|---|---|---|
| 屏幕本体最大宽 | 1100 | 超过这个宽度会显得空旷 |
| 屏幕本体最大高 | 720 | 留出上下呼吸空间 |
| 页面内边距 | 32 | Border 的 Padding |
| Header 高 | 64 | |
| Footer 高 | 72 | |
| 侧边导航宽 | 240 | |
| 导航条目高 | 44 | 键鼠点击舒适区下限 |
| 设置行高 | 56 | |
| 行内标签列宽 | 280 | |
| 行内控件列宽 | 320 | |
| 分类标题与首个设置行间距 | 16 | |
| 分类之间间距 | 32 | |

这些值不是绝对的，但**同一批数值一起用**才能形成节奏感。单独调某一项通常会破坏对齐。

---

## 五、主菜单 / 暂停菜单共用方案

"共用同一套"的正确拆法是：**屏幕本体只负责显示和收集输入，不负责决定"应用"意味着什么**。

```
W_SettingsScreen         ← 只有一套，两个宿主都用它
    ↑ 推入
W_MainMenuHost           ← 主菜单壳，推到 UI.Layer.Menu
W_PauseMenuHost          ← 暂停菜单壳，推到 UI.Layer.Menu（或 GameMenu）
```

两个宿主的差别只在这几处：

| | 主菜单 | 暂停菜单 |
|---|---|---|
| 输入模式 | Menu（纯 UI） | GameAndMenu（后面游戏可能还在跑） |
| 返回键行为 | 回主菜单 | 关掉面板、恢复游戏输入 |
| 应用语义 | 可即时生效 | 必须 pending → 应用 |

**推荐统一走 pending 模式**：内部维护一份"待提交值"，用户改的所有东西先写进待提交值，点"应用"才写进真实配置并落盘，点"取消"丢弃待提交值。这样两个宿主用同一套逻辑，主菜单下只是用户几乎感觉不到差别而已。

> 反过来做（主菜单即时生效、暂停菜单 pending）会导致屏幕里充满 `if (IsPauseMenu)` 分支，很快失控。

**关于 Apply / Save 的顺序（这块很容易出错）：**

如果以后接 `GameUserSettings`，要注意 Set 类节点只是改内存值，屏幕上不会有任何变化。必须：

1. `Apply Settings` → 让当前会话生效（从设置菜单调用时，"Check for Command Line Overrides" 传 `false`，否则玩家选择会被命令行覆盖）
2. `Save Settings` → 写入磁盘，否则重启后设置还原

只 Apply 不 Save，重启就丢；只 Save 不 Apply，当场看不见变化。另外 `Set to Defaults` 也**不会自动保存**，后面要跟上 Apply + Save。

还有一点：分辨率相关的改动在 PIE 里看不到效果，要用 **Standalone Game** 测试。

---

## 六、焦点与导航（键鼠场景）

你选了 PC 键鼠，CommonUI 的焦点系统压力比手柄小，但仍有三处必须显式配置，否则默认行为会很怪。

**1. 屏幕打开时的初始焦点**

在 `W_SettingsScreen` 的 `Event On Activated` 里手动 `Set Focus` 到想要的位置。默认焦点经常会落在左上角第一个东西上（可能是关闭按钮或某个导航条目），不一定是你想要的。

**2. 用显式 Navigation 规则**

在需要的地方设置 `Navigation` 属性，而不是依赖自动推断：

- 侧边导航内部：上下走 (`Up/Down`)，并把 `Escape` 之外的边界设成 `Stop`，防止焦点跑出导航区
- 导航 → 内容区：设置 `Right` 指向内容区的第一个可交互控件
- 内容区 → 导航：设置 `Left` 回到导航
- Footer 按钮之间：左右走

**3. 关闭行为接 Universal Back Action**

在 `CommonUIInputData` 里配好 Back/Cancel 的 Universal Input Action，然后在屏幕里处理这个动作（PC 上是 Esc）。不要自己在 PlayerController 里手动监听 Esc——那样会和 CommonUI 的路由打架，出现"按一次 Esc 关了两层"的问题。

**调试技巧**：运行中用控制台命令 `CommonUI.DumpActivatableTree` 打印当前激活栈，焦点跑飞时能立刻看出是哪一层没正确失活。

---

## 七、搭建顺序建议

按这个顺序做，每一步都能独立验证，不会堆到最后一起爆炸。

1. 在 `Project Settings → Plugins` 确认 CommonUI 已启用
2. `Project Settings → Input → Input Data` 指定一个 `CommonUIInputData` 派生对象，配好 Continue / Back 两个通用动作
3. 先做 **W_SettingsRow**，用几个假的静态行验证对齐和尺寸参数
4. 再做 **W_SettingsScreen** 的三区骨架 + `CommonActivatableWidgetSwitcher`，此时页面里放空白页，先验证焦点在三个区之间跳转正常
5. 做 **W_GameplayPage** 的实际内容，接上真实数据
6. 最后分别接 **W_MainMenuHost** 和 **W_PauseMenuHost**，验证两种输入模式下行为一致

第 3、4 步不要跳过。设置界面的返工绝大多数发生在"控件都摆完了才发现行对不齐"或者"页面切了但焦点还在旧页"。

---

## 八、几个容易踩的坑

**样式不要散落在控件里。** CommonUI 的 `CommonButtonStyle` / `CommonTextStyle` / `CommonBorderStyle` 数据资产就是为这件事设计的。按钮样式硬编码在各个 widget 里，后期统一调 UI 风格时会变成体力活。

**静态布局包一层 Invalidation Box。** 设置界面绝大部分是静态内容，用 Invalidation Box 缓存几何体，可以省掉每帧的布局重算。对设置界面这种"打开后基本不动"的界面收益明显。

**不要在 Tick 里轮询设置值。** 用事件驱动：滑块变了就派发事件，界面收到事件再更新显示。Tick 轮询在设置界面里纯属浪费，而且容易掩盖数据流的问题。

**"自定义"档位的处理。** 以后接画质档位时会遇到：如果玩家逐项调了画质，`Get Overall Scalability Level` 会返回 `-1`（表示混合状态）。你要么加一个"自定义"选项，要么让下拉框显示为选中态之外的第三态。这个坑在只有 Gameplay 设置时不会遇到，但现在规划数据结构时留个心眼。

---

## 参考资料

- [Common UI Plugin for Advanced User Interfaces in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine?application_version=5.6)
- [Design Guidelines for Using CommonUI in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/design-guidelines-for-using-commonui-in-unreal-engine?application_version=5.6)
- [UCommonActivatableWidgetSwitcher API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidgetSwitcher/?application_version=5.4)
- [Common UI: Switchers and Tabs](https://unrealist.org/commonui-switchers-and-tabs/)
- [Common UI Quickstart Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine)
- [Using CommonUI With Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-commonui-with-enhnaced-input-in-unreal-engine)
- [DPI Scaling in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/dpi-scaling-in-unreal-engine)
- [How Common UI is Setup in LyraStarterGame](https://x157.github.io/UE5/LyraStarterGame/CommonUI/)
