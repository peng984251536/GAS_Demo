# 调试开关（控制台变量）

项目所有调试输出（画线、屏幕文字）统一用控制台变量（CVar）控制，**默认全部关闭**。

- Development 测试包启动后是干净的；需要时临时打开。
- Shipping 包里这些变量不存在，调试代码被编译掉，不可能出现调试线。
- 定义集中在 `Source/GAS_Demo/Public/Debug/CC_DebugCVars.h` / `Private/Debug/CC_DebugCVars.cpp`。

## 开关一览

| 变量 | 取值 | 控制什么 |
|---|---|---|
| `cc.Debug.Messages` | 0 关 / 1 开 | C++ 屏幕调试文字：技能激活、寻敌、追击、连招与组件错误提示等。单个技能可在蓝图 Class Defaults 取消勾选 `ShowDebug` 单独屏蔽 |
| `cc.Debug.Melee` | 0 关 / 1 只画勾选的 / 2 全部 | 近战判定形状（红）与命中点（绿）。"勾选的"指动画通知 `MeleeHitBox` 上的 `bDrawDebug` |
| `cc.Debug.Projectile` | 0 关 / 1 只画勾选的 / 2 全部 | 箭命中时的碰撞球（红）与接触点（黄）。"勾选的"指箭蓝图上的 `bDrawCollisionDebug` |
| `cc.Engagement.Debug` | 0 关 / 1 开 | AI 攻击者 → 目标连线（近战红、弓兵青）、被挡住的怪头顶 "Blocked" |
| `cc.Survival.Debug` | 0 关 / 1 开 | 生存模式屏幕信息：波次、经验、存活数、复活倒计时 |

生存模式另有调参变量和命令（`cc.Survival.XPScale`、`cc.Survival.SetWave` 等），见 [SurvivalMode-Usage](../Gameplay/SurvivalMode-Usage.zh-CN.md)。

## 怎么打开

| 方式 | 生效范围 | 用法 |
|---|---|---|
| 控制台 | 本次运行，立即生效 | 游戏中按 `~`，输入 `cc.Debug.Melee 2`；输入 `cc.` 会列出全部变量 |
| 编辑器 Output Log | 同上 | Output Log 底部 Cmd 输入框 |
| 打包后的 `Engine.ini` | 这台电脑，每次启动 | 编辑 `<打包目录>/GAS_Demo/Saved/Config/Windows/Engine.ini`，在 `[SystemSettings]` 下写 `cc.Debug.Melee=2`，重启游戏 |
| 启动参数 | 本次启动 | `GAS_Demo.exe -ExecCmds="cc.Debug.Melee 2, cc.Debug.Messages 1"` |

不要在项目的 `Config/DefaultEngine.ini` 里打开这些开关：它会被打进包里，测试包就不干净了。

## 新增调试输出的约定

1. 画线、屏幕文字必须受某个 `cc.Debug.*` 开关控制，默认 0。在 `CC_DebugCVars` 里新增变量和查询函数，并在本文登记。
2. 资产上的调试 bool（如 `bDrawDebug`）只作为"只看这一个"的筛选，配合取值 1 使用；不能绕过 CVar 直接画。
3. 错误和警告先写 `UE_LOG`，屏幕文字只是附加显示。
4. 蓝图里的 Print String 不受这些开关控制，在 Development 包里仍会显示；调试用的 Print String 请调完删除，或改用日志。
