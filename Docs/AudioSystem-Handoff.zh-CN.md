# 音效系统交接（2026-10-27）

给后续对话 / 协作者的入口文档：音效系统怎么用、音效挑选台怎么用、怎么用 Python 把音效导入 UE。
更细的配置说明见 `Docs/AudioConfig.zh-CN.md`。

---

## 1. 音效系统总览

**原则：调用方只说"播放哪个事件、在哪播"；播哪个音效、怎么随机、衰减和并发，全部由配置决定。**

| 组成 | 文件 | 作用 |
| --- | --- | --- |
| 配置资产 | `Public/Audio/CC_AudioConfig.h`，资产 `/Game/GAS_Demo/Audio/DA_AudioConfig_Default` | 事件名 → 音效变体 + 播放规则；脚步地面映射。只读共享数据 |
| 项目设置 | `Public/Audio/CC_AudioSettings.h`（项目设置 → Game → CC Audio） | 指定默认配置资产 |
| 运行时子系统 | `Public/Audio/CC_AudioSubsystem.h`（GameInstance 子系统） | 启动时预加载配置；保存"上次播放的变体"等运行时状态；专用服务器不创建 |
| 播放入口 | `Public/Audio/CC_AudioLibrary.h` | `PlayAudio2D` / `PlayAudioAtLocation` / `PlayAudioAttached`，蓝图同名节点 |
| 动画通知 | `Public/Animation/Notifies/AnimNotify_PlayAudio.h`（`CC Play Audio`） | 攻击、受击、施法、落地等在动画关键帧播放 |
| 脚步通知 | `Public/Animation/Notifies/AnimNotify_Footstep.h`（`CC Footstep`） | 落脚帧向下检测地面，按物理表面类型查配置选事件 |
| 按钮基类 | `Public/UI/Framework/CC_ButtonBase.h` | 初始化时把 `UI.Hover` / `UI.Click` 写入 CommonButton 的 Sound Override |

### 1.1 代码里播放

```cpp
#include "Audio/CC_AudioLibrary.h"
UCC_AudioLibrary::PlayAudio2D(this, TEXT("UI.Click"));                                  // UI / 全局，暂停时也响
UCC_AudioLibrary::PlayAudioAtLocation(this, TEXT("Hit.Light"), Hit.ImpactPoint);       // 世界中一个点
UCC_AudioLibrary::PlayAudioAttached(TEXT("Attack.Swing"), Mesh, TEXT("hand_r"));        // 跟随组件，返回 UAudioComponent*
```
- 三个函数都有可选 `VolumeScale`。只在本机播放，不做网络同步；专用服务器自动跳过。
- 事件未配置时不报错，Output Log 警告一次：`[Audio] 事件 X 没有配置可用的音效`。

### 1.2 各类音效现在在哪里触发

| 音效 | 触发方式 | 状态 |
| --- | --- | --- |
| 攻击挥砍、弓箭、受击 | 动画上的 `CC Play Audio` Notify（Event 填 `Attack.Swing` / `Attack.Shoot` / `Hit.Light`） | 能力代码里的音效已全部移除；**Notify 需要用户自己放到蒙太奇上** |
| 脚步 | 走 / 跑动画落脚帧的 `CC Footstep` | 需要放 Notify；地面映射在配置的 `Footstep Surface Events` |
| 按钮悬停 / 点击 | 继承 `UCC_ButtonBase` 的按钮 | `UCC_MenuButton`、`UCC_BuffOfferCardWidget`（点击用 `UI.Confirm`）已接入 |
| UI.Open / Close / Cancel / Error / Toggle | 无 | **未接入**。建议在 `CC_RootLayout` 菜单层开关时统一播放 |

Notify 放在动画上的好处：蒙太奇同步到所有客户端，每台机器都触发，联机时所有玩家都能听到；动画编辑器里拖时间轴也能预览。

### 1.3 每个事件的播放规则（`DA_AudioConfig_Default` → Events）

`Sounds`（变体）、`Volume Range` / `Pitch Range`（每次随机）、`Avoid Repeat`（不连续重复，默认开）、
`Attenuation`（3D 衰减，空则用配置的 `Default Attenuation`）、`Concurrency`（同时最多几个）。

### 1.4 脚步地面区分

项目设置 → Physics → Physical Surface 命名表面类型 → 新建 Physical Material 选该类型并挂到地面材质 / Landscape Layer Info →
配置的 `Footstep Surface Events` 加映射（如 Grass → `Footstep.Grass`）。射线为复杂碰撞检测，材质上的 Phys Material 可被读到。

---

## 2. 用 Python 在 UE 里加载音效资源

三个脚本都在 `Scripts/`，在编辑器里用 **工具 → 执行 Python 脚本**，或 Output Log 选 **Cmd** 模式输入
`py D:/UEProject/GAS_Demo/Scripts/<脚本名>.py`（注意 `py` 前缀和正斜杠；不要在 Python 模式下输入路径，也不要漏 `py`）。

| 脚本 | 何时用 | 做什么 | 成功标志 |
| --- | --- | --- | --- |
| `configure_audio_starter.py` | 最初建库（已运行过） | 导入 `SourceArt/Audio/CartoonStarterKit` 的 82 条音效，创建 `DA_AudioConfig_Default`；只补缺，不覆盖 | `AUDIO_CONFIG_SETUP_OK` |
| `setup_audio_rules.py` | 第一次使用新播放规则时运行一次 | 创建 `/Game/GAS_Demo/Audio/Rules/ATT_SFX_Default`（衰减）、`SC_Footstep`(8)、`SC_Hit`(6)，给 Footstep/Hit/Attack 事件填随机音量音高；只补默认值 | `AUDIO_RULES_OK` |
| `apply_audio_picks.py` | 每次从挑选台确定新音效后 | 读 `SourceArt/Audio/Selected/manifest.json`，导入到 `/Game/GAS_Demo/Audio/Selected/`（覆盖同名），把清单中事件的 Sounds **替换**为挑选结果；首次运行备份到 `Saved/Audio/AudioConfig_BeforePicks.json` | `AUDIO_PICKS_APPLIED` |

恢复挑选前的配置：`py D:/UEProject/GAS_Demo/Scripts/apply_audio_picks.py --restore`。

> 注意：`setup_audio_rules.py` 与 `apply_audio_picks.py` 写好后只做了语法检查，尚未在 UE 里实际跑通过；若报错，看 `Saved/Logs/GAS_Demo.log`。

---

## 3. 音效挑选台

- 页面：claude.ai 上的 Artifact **"GAS_Demo 音效挑选台"**，URL `https://claude.ai/artifact/9tgmsP8PGvyePT7FWuLhrE`（私有，仅本人）。
- 功能：按事件分组试听（响度对齐、连播、键盘 ↑↓ 空格 K X ←→），每条"保留 / 淘汰"，可"改到…"其他事件，事件备注。
- 数据库（Artifact 的 db 能力，Claude 用 ArtifactData 工具读写）：
  - `picks/<id>`：`{event, verdict: "keep"|"drop", assign?: 改到的事件, file, source}`
  - `notes/<事件名>`：`{text}`
- 候选来源（`SourceArt/Audio` 下）：`SwordSwings_Clean_20261026`（12 个挥砍，归 Attack.Swing）、`AtelierMagicae`（113 个奇幻 UI，自动猜测归类）、
  `CartoonStarterKit`（现用 + Kenney 原始素材）、`RealisticUI`、`CoreGameplay`，以及程序合成样品（只在旧工作区里，可用 `Tools/AudioPicker/gen.py` 重新生成）。
- 截至 2026-10-27 01:00 的"保留"（已生成到 `SourceArt/Audio/Selected/`，9 条）：
  Attack.Swing = `SFX_SwordSwing_02/03/04/05.wav`（新挥砍包）；Footstep.Concrete = Kenney `footstep02.ogg`、`footstep05.ogg`；
  Footstep.Grass = `SFX_Footstep_Grass_04.wav`；UI.Click = `Fantasy_UI (1).wav`、Kenney `click1.ogg`。
  **在挑选台点"保留"不会自动进游戏**，每次都要走下面 3.1 的流程重新生成 `Selected/`，再在 UE 里运行 `apply_audio_picks.py`。
  `make_selected.py` 也可以直接在用户电脑上跑（已装 scipy），`--src` 指向项目里的 `SourceArt/Audio` 即可，id 规则不变。

### 3.1 从挑选结果到游戏里（让新对话里的 Claude 做 1～3）

1. 用 ArtifactData `list` 读 `picks` 集合（`out_dir` 存成 JSON），整理成 `picks.json`（`[{id, data}]`）。
2. 把 `SourceArt/Audio` 打包传到 Claude 工作区，运行
   `python3 Tools/AudioPicker/make_selected.py --picks picks.json --src <SourceArt/Audio 副本> --out <输出>`。
3. 把输出（WAV + `manifest.json`）放回 `SourceArt/Audio/Selected/`。
4. 用户在 UE 里运行 `apply_audio_picks.py`。

### 3.2 给挑选台加新素材 / 重新发布

1. 素材放进 `SourceArt/Audio/<新文件夹>`。整包属于一个事件且文件名无需猜测时，在 `Tools/AudioPicker/build.py` 的 `EXTRA_FOLDERS` 加一行；
   需要逐条猜测归类的（如 AtelierMagicae），参照 `atelier.py` 的写法。
2. 在 Claude 工作区：`python3 build.py --src <SourceArt/Audio 副本> --site site`，并把 `Tools/AudioPicker/site/index.html` 复制到 `site/`。
3. 用 Artifact 工具先 `read` 上面的 URL，再带 `url` 发布 `site/index.html`，`files` 包含 `manifest.json` 和 `audio/*.json`（共 27 个事件文件），保持 `capabilities: {db: {}}`。
4. **候选 id = sha1("aud/src/" + 相对 SourceArt/Audio 的路径) 前 10 位**，不要改这个规则，否则已有的保留 / 淘汰记录会对不上。

---

## 4. 素材授权

- Kenney（CartoonStarterKit 来源）：CC0。
- Atelier Magicae Fantasy UI SFX（`SourceArt/Audio/AtelierMagicae`）：可商用、可修改，**不得再分发文件**，署名非必须。原始 rar 在用户"下载"文件夹。
- `SwordSwings_Clean_20261026`：用户自行添加，来源与授权未记录，使用前请确认。
- `SourceArt/` 与 `Content/` 被 `.gitignore` 排除，素材不进源码仓库。

## 5. 未完成 / 待确认

- 最近几轮 C++ 改动（CC_AudioSubsystem、播放入口、CC Play Audio、脚步全局映射、移除能力内音效）**尚未编译验证**。
- 在攻击、弓箭、受击蒙太奇上放 `CC Play Audio`；`Attack.Shoot` 事件若配置里不存在需新建，或改用 `Attack.Swing`。
- 霸体 / 死亡不播放受击蒙太奇 → 没有受击声；如需每次命中都响，改在 `CC_GameplayCue_HitImpact` 播放（两处只留一处）。
- `WBP_CombatMain` 的 `Button_Character` 是原生 UButton，无音效；改为 `UCC_ButtonBase` 子类或在 Style 里配。
- UI.Open / Close 等未接入（建议在 `CC_RootLayout`）。
- 走跑若在 BlendSpace 混合，Notify Trigger Mode 设为 Highest Weighted Animation，避免一步响两次。
