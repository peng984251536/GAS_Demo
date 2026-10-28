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

## 播放音效（推荐写法）

**调用方只说"播放哪个事件、在哪播"**，播哪个变体、音量音高怎么随机、衰减和并发怎么限制，全部由配置里该事件的规则决定。三个入口都在 `UCC_AudioLibrary`，蓝图里搜索同名节点即可，事件名有下拉框。

| 场景 | C++ | 说明 |
| --- | --- | --- |
| UI / 全局提示 | `UCC_AudioLibrary::PlayAudio2D(this, "UI.Click");` | 无空间位置；按 UI 音效处理，游戏暂停时也会响 |
| 世界中一个点 | `UCC_AudioLibrary::PlayAudioAtLocation(this, "Hit.Light", Hit.ImpactPoint);` | 命中点、落脚点、爆炸点 |
| 跟随物体 | `UCC_AudioLibrary::PlayAudioAttached("Attack.Swing", Character->GetRootComponent());` | 返回 `UAudioComponent*`，需要时可提前 `Stop()` |

```cpp
#include "Audio/CC_AudioLibrary.h"

UCC_AudioLibrary::PlayAudioAtLocation(this, TEXT("Hit.Heavy"), GetActorLocation());
```

- 三个函数都有可选的 `VolumeScale`，叠乘在事件音量之上（例如潜行时调小）。
- 只在本机播放，不做网络同步；专用服务器上自动跳过。联机要让所有人听到，就在每台机器都会执行的地方调用（动画 Notify、GameplayCue、OnRep）。
- 事件没配置或没有可用音效时不会报错，只在 Output Log 里警告一次：`[Audio] 事件 X 没有配置可用的音效`。
- 动画里的音效优先用 `CC Play Audio` Notify，不需要写代码。
- 只想拿到 `USoundBase`（例如交给 CommonButton 的 Sound Override）时，用 `PickGlobalAudioSound`。

### 每个事件可配置的规则（`DA_AudioConfig_Default` → Events → 某个事件）

| 字段 | 作用 | 建议 |
| --- | --- | --- |
| `Sounds` | 变体列表，随机选一个 | 脚步、受击每种 3～5 个 |
| `Volume Range` / `Pitch Range` | 每次播放随机的音量 / 音高倍率 | 脚步、受击音高 0.95～1.05 |
| `Avoid Repeat` | 不连续两次播同一个变体 | 默认开 |
| `Attenuation` | 3D 衰减；为空用配置的 `Default Attenuation` | UI 事件不用填 |
| `Concurrency` | 同时最多播几个 | 脚步 8、受击 6 |

第一次使用时运行一次 `py D:/UEProject/GAS_Demo/Scripts/setup_audio_rules.py`，会自动创建默认衰减和脚步 / 受击的并发资产，并给战斗类事件填上随机范围（只补默认值，不覆盖手动改过的）。

### 结构

- `UCC_AudioConfig`（数据资产，只读）：事件 → 变体 + 播放规则；脚步表面映射。
- `UCC_AudioSubsystem`（GameInstance 子系统）：启动时预加载配置；保存"上次播放的变体"等运行时状态。专用服务器不创建。
- `UCC_AudioLibrary`（静态函数库）：唯一播放入口。动画编辑器预览等没有 GameInstance 的场合会退回无状态选音，仍能出声。

## 加载和打包

项目设置保存软引用，全局读取在首次访问时同步加载资产，并由设置对象保留强引用，避免配置被垃圾回收。默认资产通过 `Sounds` 的硬引用保留对应音效。第一次访问会加载配置和这些音效；`UCC_AudioSubsystem` 在 GameInstance 初始化时就完成这次加载，所以战斗中第一次播放不会卡顿。

`Config/DefaultGame.ini` 将 `/Game/GAS_Demo/Audio` 加入 `DirectoriesToAlwaysCook`，确保仅通过全局设置引用的数据资产与音效会被打包。若把配置移动到其他目录，也需调整此打包目录或使用项目的 Asset Manager Cook 规则。

## 已接入的播放点

| 位置 | 怎么配置 | 播放方式 |
| --- | --- | --- |
| 动画 Notify `CC Play Audio`（`UAnimNotify_PlayAudio`） | 在攻击 / 受击 / 施法 / 落地等动画关键帧放置，填 `Event`（下拉选择） | 勾 `Follow Mesh` 附着在网格体（可选 `Socket Name`）跟随播放；不勾则在该位置播一次 |
| 脚步 Notify `CC Footstep`（`UAnimNotify_Footstep`） | 走 / 跑动画落脚帧放置；地面映射在全局配置的 `FootstepSurfaceEvents` + `DefaultFootstepEvent`；可选 `EventOverride` | 从脚骨向下检测地面，按物理表面类型查全局表，在落脚点播放 |
| 按钮基类 `UCC_ButtonBase` | `HoverSoundEvent` / `ClickSoundEvent`（默认 `UI.Hover` / `UI.Click`） | 初始化时写入 CommonButton 的 Hovered / Pressed Sound Override |

- 攻击、受击音效**不再由能力代码播放**（`CC_Primary` / `CC_BowShoot` / `CC_HitReact` 已移除音效属性），统一放在动画上：
  - 近战：在攻击蒙太奇刀挥出的那一帧放 `CC Play Audio`，`Event = Attack.Swing`，`Socket Name` 可填武器插槽；
  - 弓箭：在射击蒙太奇的放箭帧（`Bow Release` 旁边）放一个，`Event = Attack.Shoot`（或 `Attack.Swing`）；
  - 受击：在受击蒙太奇第一帧附近放一个，`Event = Hit.Light`。
- 蒙太奇会同步到所有客户端，Notify 在每台机器上都触发，所以联机时所有玩家都能听到（专用服务器自动跳过）。
- 在动画编辑器里拖动时间轴预览时也会出声，方便对齐。
- 霸体、死亡时不会播放受击蒙太奇，所以也没有受击声。若需要"任何命中都有声音"，可改在 `CC_GameplayCue_HitImpact`（命中特效 Cue）里播放；两处只保留一处，避免重复。
- `UCC_MenuButton`、`UCC_BuffOfferCardWidget`（点击用 `UI.Confirm`）已继承 `UCC_ButtonBase`。新按钮蓝图请以 `UCC_ButtonBase` 为父类；UMG 原生 `UButton` 不会自动带音效。
- 按钮的音效在初始化时选定一个变体；需要每次点击都随机时，在配置里填带 Random 节点的 Sound Cue。
- 脚步的地面映射只在全局配置 `DA_AudioConfig_Default` 里维护，流程见下文"脚步声配置流程"。

## 初始素材和复现

原始下载、授权、WAV 和初版试听页位于 `SourceArt/Audio/CartoonStarterKit`。`Scripts/configure_audio_starter.py` 导入初版并创建 `DA_AudioConfig_Default`，运行初版验证时需将项目设置切回该资产。`Scripts/prepare_realistic_ui.py` 从现有 CC0 素材生成新版 UI；`Scripts/configure_realistic_ui.py` 导入新版并复制原配置到 `DA_AudioConfig_RealisticUI`，替换其中的 UI 事件。

`Content` 和 `SourceArt` 都被项目现有 `.gitignore` 排除；资产已保存在本机，但源码 Git 仓库不会自动包含这些二进制文件。换机器时需要同步素材及资产，或同步源素材后重新运行导入脚本。

验证记录：编译日志 `Saved/BuildLogs/AudioConfig-Build.log`；资产配置报告 `Saved/Audio/AudioConfigSetup.json`，包括默认配置的全局读取、所有事件取音效、缺失事件和空变体的检查。

## 用挑选结果替换音效

1. 在音效挑选台（claude.ai 上的 "GAS_Demo 音效挑选台" 页面）里给事件点"保留"。
2. 让 Claude 读取挑选结果：它会把选中的声音加工（裁静音、统一峰值；3D 用途转单声道）后放到 `SourceArt/Audio/Selected/`，并更新其中的 `manifest.json`。
3. 打开编辑器，在 Output Log 切到 Python 执行 `py "Scripts/apply_audio_picks.py"`。
   - 导入到 `/Game/GAS_Demo/Audio/Selected/`，同名资产覆盖。
   - manifest 中出现的事件，`Sounds` 被**替换**为挑选结果；其余事件不变。
   - 第一次运行会把这些事件原来的音效备份到 `Saved/Audio/AudioConfig_BeforePicks.json`；执行 `py "Scripts/apply_audio_picks.py" --restore` 可恢复。
4. 新素材 `SourceArt/Audio/AtelierMagicae`（Atelier Magicae Fantasy UI SFX）允许商用、可修改，但不得再分发，见同目录 `LICENSE_NOTE.md`。

## 脚步声配置流程

思路：**动画只负责"什么时候踩下去"，踩到什么地面、放什么声音全部由全局配置决定。** 新增一种地面只改配置，不用回头改每个动画。

1. **命名表面类型**（一次性）：项目设置 → Engine → Physics → Physical Surface，给 SurfaceType1、SurfaceType2… 起名，例如 Grass、Wood、Snow。
2. **给地面指定物理材质**：
   - 新建 Physical Material 资产（如 `PM_Grass`），Surface Type 选 Grass；
   - 普通网格：在地面材质（Material）的 Phys Material 里指定，或在网格体组件的 Phys Material Override 里指定；
   - 地形 Landscape：在对应 Landscape Layer Info 资产上指定 Phys Material，这样不同图层自动对应不同地面。
3. **在全局配置里做映射**：打开 `/Game/GAS_Demo/Audio/DA_AudioConfig_Default`
   - `Footstep Surface Events` 里加一行：Grass → `Footstep.Grass`；
   - `Default Footstep Event` 是没配置或没检测到地面时的兜底（默认 `Footstep.Concrete`）；
   - 事件名必须在同一资产的 `Events` 里有音效，否则不发声。
4. **在动画里放 Notify**：走 / 跑动画左右脚落地帧各放一个 `CC Footstep`，`Foot Bone` 填脚骨名。一般不用改别的；落地等特殊动作可以填 `Event Override`（如 `Movement.Land`）强制指定事件。
5. **验证**：PIE 中在不同地面上行走。若始终是默认声音，按顺序检查：物理材质是否真的挂在地面上 → Surface Type 是否选对 → 全局表里是否有这一行 → `Events` 里是否有该事件的音效。
