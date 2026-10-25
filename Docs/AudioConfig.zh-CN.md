# 全局音效配置

`UCC_AudioConfig` 是音效数据资产。当前默认实例为 `/Game/GAS_Demo/Audio/DA_AudioConfig_RealisticUI`，在 **项目设置 → Game → CC Audio → Default Audio Config** 中指定。各关卡、角色、UI 和 C++ 代码可以通过同一个全局入口读取，不需要把资产逐个传给使用者。

当前版本将 8 种 UI 事件的 18 条声音换成较轻的纸张、衣料、木质和机械按键拟音，其余事件沿用初版。原版 `/Game/GAS_Demo/Audio/DA_AudioConfig_Default` 保留，可在项目设置中切回。新版试听位于 `SourceArt/Audio/RealisticUI/试听.html`，新旧对比音频为同目录的 `Compare_OldThenNew.wav`。这是一版待试听确认的风格调整。

## 数据内容

`Events` 是一个名称到音效变体数组的 Map。默认提供 27 种事件、82 条音效；添加新事件只需在资产中添加键，不用改 C++。

常用名称：

| 类型 | 事件名称 |
| --- | --- |
| UI | `UI.Hover`、`UI.Click`、`UI.Confirm`、`UI.Cancel`、`UI.Error`、`UI.Open`、`UI.Close`、`UI.Toggle` |
| 攻击 | `Attack.Swing`、`Attack.DrawWeapon`、`Attack.Chop` |
| 受击 | `Hit.Light`、`Hit.Heavy`、`Hit.Critical`、`Hit.Block` |
| 脚步 | `Footstep.Concrete`、`Footstep.Grass`、`Footstep.Wood`、`Footstep.Carpet`、`Footstep.Snow` |
| 移动 | `Movement.Jump`、`Movement.Land`、`Movement.Dodge` |
| 拾取/技能 | `Reward.Coins`、`Reward.Pickup`、`Reward.LevelUp`、`Skill.Cast` |

每个事件的 `Sounds` 可以填 Sound Wave 或 Sound Cue。取音效时随机选一个有效变体；缺失配置、未配置事件、全部为空的数组返回空音效。音量、音高和空间衰减由 UE 播放节点和音效资产设置控制。

共享配置仅用于保存默认数据；蓝图只读。运行时个别角色或按钮的音量变化应在播放参数上处理，避免改动共享资产。

## 蓝图调用

读取全部数据：搜索 **Get Audio Config**，取得默认数据资产，然后读取 `Events`。也可以对自己指定的配置资产调用 `Pick Sound`。

播放一个事件：

1. 调用 **Pick Global Audio Sound**，填写 `Event`，例如 `UI.Click`、`Hit.Heavy` 或 `Footstep.Grass`。
2. 对返回的 `Sound` 做 `Is Valid` 检查。
3. UI 接 `Play Sound 2D`；角色、攻击、脚步接 `Play Sound at Location` 或附着播放节点。
4. 按需要调整播放节点的音量和音高。

随机选择节点有执行引脚，每次调用选一次；`Get Audio Config` 是纯读取节点。配置访问和随机选音本身不会播放，也不负责网络同步。多人游戏中由相应的客户端表现逻辑或 GameplayCue 决定何时播放。

## C++ 调用

```cpp
#include "Audio/CC_AudioLibrary.h"
#include "Kismet/GameplayStatics.h"

if (USoundBase* Sound = UCC_AudioLibrary::PickGlobalAudioSound(
    TEXT("Hit.Heavy")))
{
    UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
}
```

也可对自定义的 `UCC_AudioConfig` 实例调用 `PickSound`，为特定武器或角色使用独立配置。调用应在游戏线程执行。

## 加载和打包

项目设置保存软引用，全局读取在首次访问时同步加载资产，并由设置对象保留强引用，避免配置被垃圾回收。默认资产通过 `Sounds` 的硬引用保留对应音效。因此第一次访问会加载配置和这些音效，建议在加载阶段或初始化时预先调用 `Get Audio Config`，不要首次在高频战斗事件里加载。

`Config/DefaultGame.ini` 将 `/Game/GAS_Demo/Audio` 加入 `DirectoriesToAlwaysCook`，确保仅通过全局设置引用的数据资产与音效会被打包。若把配置移动到其他目录，也需调整此打包目录或使用项目的 Asset Manager Cook 规则。

## 已接入的播放点

| 位置 | 事件属性（类默认值可改，设为 None 不播放） | 默认事件 | 播放方式 |
| --- | --- | --- | --- |
| 近战攻击 `UCC_Primary` | `AttackSoundEvent` | `Attack.Swing` | 蒙太奇开始后附着在角色上播放 |
| 弓箭攻击 `UCC_BowShoot` | `ReleaseSoundEvent` | `Attack.Swing` | 收到放箭事件时播放 |
| 受击 `UCC_HitReact` | `HitSoundEvent` | `Hit.Light` | 受击反应开始时播放 |
| 按钮基类 `UCC_ButtonBase` | `HoverSoundEvent` / `ClickSoundEvent` | `UI.Hover` / `UI.Click` | 初始化时写入 CommonButton 的 Hovered / Pressed Sound Override |
| 脚步 Notify `CC Footstep`（`UAnimNotify_Footstep`） | `DefaultEvent` + `SurfaceEvents` | `Footstep.Concrete` | 从脚骨向下检测地面，按物理表面类型选事件，在落脚点播放 |

- 能力里的播放统一走 `UCC_GameplayAbility::PlayAudioEventOnAvatar`，**只在执行该能力的机器上播放，不做网络同步**：
  玩家攻击在本机客户端和服务器上播放；AI 攻击、弓箭和受击主要在服务器执行，联机时只有主机能听到。需要所有客户端都听到时，改用 GameplayCue 或动画通知。
- 霸体、死亡时 `UCC_HitReact` 会跳过，此时没有受击声。`CC_GameplayCue_HitImpact` 的 `HitSound` 应留空，避免和受击能力重复播放。
- `UCC_MenuButton`、`UCC_BuffOfferCardWidget`（点击用 `UI.Confirm`）已继承 `UCC_ButtonBase`。新按钮蓝图请以 `UCC_ButtonBase` 为父类；UMG 原生 `UButton` 不会自动带音效。
- 按钮的音效在初始化时选定一个变体；需要每次点击都随机时，在配置里填带 Random 节点的 Sound Cue。
- 脚步 Notify 的表面类型需在 **项目设置 → Physics → Physical Surface** 命名，并给地面的物理材质选择对应类型，再在 Notify 的 `SurfaceEvents` 里映射（如 `SurfaceType1 → Footstep.Grass`）。动画在每台机器上都会播放，所以脚步声不需要同步。

## 初始素材和复现

原始下载、授权、WAV 和初版试听页位于 `SourceArt/Audio/CartoonStarterKit`。`Scripts/configure_audio_starter.py` 导入初版并创建 `DA_AudioConfig_Default`，运行初版验证时需将项目设置切回该资产。`Scripts/prepare_realistic_ui.py` 从现有 CC0 素材生成新版 UI；`Scripts/configure_realistic_ui.py` 导入新版并复制原配置到 `DA_AudioConfig_RealisticUI`，替换其中的 UI 事件。

`Content` 和 `SourceArt` 都被项目现有 `.gitignore` 排除；资产已保存在本机，但源码 Git 仓库不会自动包含这些二进制文件。换机器时需要同步素材及资产，或同步源素材后重新运行导入脚本。

验证记录：编译日志 `Saved/BuildLogs/AudioConfig-Build.log`；资产配置报告 `Saved/Audio/AudioConfigSetup.json`，包括默认配置的全局读取、所有事件取音效、缺失事件和空变体的检查。
