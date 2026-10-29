# 提示弹窗（Toast）

战斗中不抢输入、自动消失的提示：升级、波次开始、下一波预告、队友倒下/复活等。
建立在 [UIFramework.zh-CN.md](UIFramework.zh-CN.md) 的**通知层**之上（RootLayout 最上层、HitTestInvisible），不进页面栈、不暂停、不改输入模式。

> 需要玩家点"确定/取消"的弹窗不是 Toast，请用 `UI.Layer.Modal` + `CC_ActivatableWidget`（参考 `CC_QuitDialogWidget`）。

## 1. 一句话调用（静态函数）

**C++**

```cpp
#include "UI/Toast/CC_ToastStatics.h"

UCC_ToastStatics::ShowToastText(this,
    NSLOCTEXT("CCToast", "LvUp", "全队升级"),
    NSLOCTEXT("CCToast", "LvUpMsg", "等级提升至 Lv.5"),
    ECC_ToastStyle::Success);
```

**蓝图**：任意蓝图里搜 `Show Toast Text`（分类 CC | Toast），World Context 自动填 self。需要优先级、图标、音效时用 `Make CC_ToastRequest` → `Show Toast`。清空用 `Clear Toasts`。

第一个参数（WorldContext）决定发给谁：

| 传入的对象 | 发给 |
| --- | --- |
| 本地 PlayerController / 本地玩家控制的 Pawn 或其组件 / 属于本地玩家的 Widget | 只发给这个玩家 |
| 其他对象（GameState、GameMode、AI、子系统……） | 本机所有本地玩家（分屏每人一份） |

只影响本机界面：在专用服务器上调用不显示任何东西，在主机上调用也不会出现在其他客户端。需要所有人都看到的事件按第 5 节走对局消息。

`FCC_ToastRequest` 字段：

| 字段 | 说明 |
| --- | --- |
| Title / Message | 标题、正文；都为空时请求被忽略 |
| Style | Info / Success / Warning / Danger，决定强调色（`Border_Accent`） |
| Duration | 完全显示的秒数（不含进出场动画）；<=0 用默认 3 秒。真实时间 |
| Priority | 排队时大的先出；同优先级先到先出 |
| DedupeKey | 非空时与显示中/排队中的同键提示合并（刷新内容、重新计时、播 `Anim_Refresh`），用于连续升级、倒计时刷新 |
| SourceTag | 来源标签，只给条目蓝图切图标/颜色用 |
| Icon | 可选图标，条目里有 `Image_Icon` 时显示 |
| SoundEvent | 真正显示（不是入队）时用 `UCC_AudioLibrary::PlayAudio2D` 播放 |

## 2. 堆栈规则

```text
            ┌──────────────┐  ← 最早的一条，到时间后往上淡出
            ├──────────────┤
            ├──────────────┤  ← 最新的一条从下方升上来淡入
            └──────────────┘
   队列：[ ... 等待中的消息，按优先级排序 ... ]
```

- 新消息**追加在最下面**，旧的被往上推；同时最多显示 **3 条**（宿主 `Max Visible`）。
- 满了就进队列（最多 `Max Queued` = 16 条，超出丢弃最低优先级里最早的）；有一条完全消失后补上。
- 队列里还有消息时，最早那条显示满 `Min Display Time When Queued`（1 秒）就提前退场让位，避免越积越多；设为 0 则严格等每条播完。
- 退场中的条目仍占位，直到 `Anim_Out` 播完才移除，所以不会出现 4 条同时在屏幕上。
- 暂停菜单、Buff 三选一等 Menu/Modal 层打开时（项目设置 `Hold While Menu Open`，默认开），整个堆栈隐藏并停止计时，关闭后接着播。
- 切图时自动清空，旧图的提示不会带到新图；刚进图根布局未就绪时先缓存，就绪后补发。

## 3. 在编辑器里做提示（含从下往上淡入淡出动画）

编译后不做任何蓝图也能用：会使用 C++ 示例卡片，并由代码做同样的上浮淡入淡出。要用自己的美术和动画：

### 3.1 条目 WBP

1. 内容浏览器 → 用户界面 → 控件蓝图，父类选 **CC_ToastEntryWidget**，例如命名 `WBP_Toast`。
2. 设计器里搭卡片。下面这些控件**按名字自动绑定**，都是可选的，缺了不报错：
   - `TextBlock_Title`、`TextBlock_Message`（文本）
   - `Image_Icon`（图像，请求带 Icon 时显示，否则折叠）
   - `Border_Accent`（边框，会被设为风格强调色，可做左侧色条或底色）
3. 动画面板（Animations）里按下面的名字新建动画，名字必须一致：

   | 动画名 | 作用 | 建议关键帧（对根控件，或对卡片外层的 Overlay/Border） |
   | --- | --- | --- |
   | `Anim_In` | 进场：从下往上淡入 | 0s：Render Transform → Translation Y = 40，Render Opacity = 0<br>0.25s：Translation Y = 0，Opacity = 1 |
   | `Anim_Out` | 退场：继续往上淡出 | 0s：Translation Y = 0，Opacity = 1<br>0.3s：Translation Y = -30，Opacity = 0 |
   | `Anim_Refresh` | 可选：同键合并刷新时播一次 | 例如 Scale 1 → 1.06 → 1 |

   - 动画长度就是进出场时长，系统按动画长度计时，不需要另外配置。
   - `Anim_Out` 的最后一帧必须是**完全透明**，因为播完后条目直接被移除。
   - 曲线可以在 Curve 编辑器里改成缓出（Ease Out），效果会更顺。
4. 项目设置 → Game → **CC Toast** → `Entry Widget Class` 选 `WBP_Toast`。

只做了其中一个动画也可以：缺的那个会自动使用代码兜底（类默认值 `Fallback Intro Time` / `Fallback Outro Time` / `Fallback Rise Distance`）。

可选的蓝图事件：
- `On Toast Applied(Request, AccentColor, bMerged)`：内容写入后；可按 `Request.SourceTag` 或 `Style` 换图标、颜色。
- `On Toast Phase Changed(Phase)`：Entering / Showing / Leaving 各触发一次。

### 3.2 堆栈位置和条数（可选）

默认堆栈在屏幕下方居中、离底部 220 像素。要改位置或条数：

1. 新建控件蓝图，父类 **CC_ToastHostWidget**，例如 `WBP_ToastStack`。
2. 放一个铺满的 Canvas 或 Overlay，再放一个名为 `Panel_Stack` 的**垂直框（Vertical Box）**，用锚点摆到想要的位置（例如右下角）。
3. 类默认值：`Max Visible`（同时显示条数，默认 3）、`Max Queued`、`Default Duration`、`Min Display Time When Queued`、`Entry Spacing`、`Entry Class`。
4. 项目设置 → CC Toast → `Host Widget Class` 选 `WBP_ToastStack`。

不要把任何控件设成可点击；整个通知层是 HitTestInvisible。

## 4. 生存模式自带的提示

`UCC_SurvivalToastRelay`（WorldSubsystem）把服务器原本就在发的 `Survival.Msg.*` 对局消息转成提示，生存模式代码没有改动：

| 时机 | 默认文案 | 风格 | 优先级 |
| --- | --- | --- | --- |
| `Survival.Msg.MatchStarted` | 生存开始 / 坚持下去，活得越久越强 | Info | 10 |
| `Survival.Msg.WaveStarted` | 第 {Value} 波 / 敌人来袭！ | Warning | 20 |
| `UI.Toast.WaveIncoming`（客户端本地，下一波前 5 秒） | 敌人即将来袭 / {Value} 秒后第 {Wave} 波开始 | Warning | 1 |
| `Survival.Msg.LevelUp` | 全队升级 / 等级提升至 Lv.{Value}（连续升级合并） | Success | 5 |
| `Survival.Msg.PlayerDown` | {Player} 倒下了 / 稍后将自动复活（自己倒下不弹） | Danger | 0 |
| `Survival.Msg.PlayerRevived` | {Player} 已复活（自己不弹） | Success | 0 |
| `Survival.Msg.MatchEnded` | 默认不弹（结算页会打开） | — | — |

模板参数：`{Value}` = 消息 Magnitude 取整，`{Player}` = Target（没有则 Instigator）的玩家名，`{Wave}` = 下一波序号。

改文案、屏蔽或新增：项目设置 → CC Toast → **Message Toasts** 按标签加一行，会覆盖同标签的内置默认；`bEnabled` 关掉即屏蔽。`Use Builtin Message Toasts` 关掉后只显示表里配置的。`Wave Incoming Lead Seconds` 设 0 关闭下一波预告。

## 5. 新增一种全员可见的提示（例：精英怪刷新）

1. `CC_SurvivalTags` 加 `Survival.Msg.EliteSpawned` 原生标签。
2. 服务器在刷出精英时调用 `GameState->BroadcastMatchMessage(CCSurvivalTags::MsgEliteSpawned, nullptr, nullptr, Count)`。
3. 项目设置 → CC Toast → Message Toasts 加一行：Key 选该标签，Title「精英怪出现」，Message「数量 {Value}」，Style = Danger。

不需要改 UI 代码。只在本机提示的事情（例如"背包已满"）直接调用第 1 节的静态函数。

## 6. 代码结构

```text
UCC_ToastStatics::ShowToast / ShowToastText（静态入口）
  └─▶ UCC_ToastSubsystem（每个本地玩家一个）
        │ 首次使用时 RootLayout.ShowNotification(HostClass, 0) 挂载；Ticker 按真实时间驱动
        ▼
      UCC_ToastHostWidget（Panel_Stack：最多 3 条 + 优先级队列）
        └─ UCC_ToastEntryWidget × N（播放 Anim_In / Anim_Out / Anim_Refresh）

ACC_SurvivalGameState.OnMatchMessage + UCC_WaveDirectorComponent.OnWaveStateChanged
  └─▶ UCC_SurvivalToastRelay → 按模板格式化 → UCC_ToastSubsystem
```

| 类 | 文件（`Source/GAS_Demo/.../UI/Toast/`） |
| --- | --- |
| `UCC_ToastStatics` | `CC_ToastStatics.h` |
| `FCC_ToastRequest` / `FCC_ToastTemplate` / `FCC_ToastQueue` | `CC_ToastTypes.h` |
| `UCC_ToastSubsystem` | `CC_ToastSubsystem.h` |
| `UCC_ToastHostWidget` | `CC_ToastHostWidget.h` |
| `UCC_ToastEntryWidget` | `CC_ToastEntryWidget.h` |
| `UCC_ToastSettings`（项目设置 CC Toast） | `CC_ToastSettings.h` |
| `UCC_SurvivalToastRelay` | `CC_SurvivalToastRelay.h` |

## 7. 验证

自动化测试（Session Frontend → Automation，筛选 `GASDemo.UI.Toast`）：
- `GASDemo.UI.Toast.Queue`：优先级、同优先级顺序、合并键、容量淘汰。
- `GASDemo.UI.Toast.Template`：命名参数格式化、内置默认模板。

PIE 手动检查：
1. 在任意 Actor 的 BeginPlay 里连续调用 5 次 `Show Toast Text`：同时只出现 3 条，从下往上依次淡入；最早的约 1 秒后往上淡出，后面的依次补上。
2. 做好 `WBP_Toast` 的 `Anim_In` / `Anim_Out` 并填到项目设置：进出场改为编辑器动画；删掉 `Anim_Out` 时退场回到代码兜底。
3. 进生存模式：「生存开始」→「第 1 波」；下一波前 5 秒出现「敌人即将来袭」。
4. 快速连续升级只保留一条，数字刷新。
5. 升级后弹 Buff 选择：提示隐藏；选完继续，不盖在选择页上。
6. 两个 PIE 客户端：一方倒下，另一方看到「XX 倒下了」，自己不弹。
7. 回大厅再进：没有残留提示。

## 8. 现状与限制

- 只做了源码静态检查，**尚未编译和 PIE 验证**。
- 条目被移除时，上方条目直接下移、新条目出现时上方直接上移（VerticalBox 不带补位动画）；新条目的 `Anim_In` 会掩盖大部分跳动。需要平滑推移时可以后续在宿主里加位移插值。
- 原生示例卡片只是占位，字号和位置按 1080p 调。
