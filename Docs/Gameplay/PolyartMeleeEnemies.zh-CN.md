# Polyart 近战敌人

资产根目录：`/Game/GAS_Demo/Enemy`。

每个角色目录包含 `BP_Enemy_<角色名>` 与 `DA_CharacterConfig_<角色名>`。角色蓝图复制自 `BP_CC_Enemy02`，保留原 AI、感知、血条和事件逻辑；动画蓝图沿用 `BP_Boris` 的图表结构，替换为 Polyart 骨骼与动画。

| 动作类型 | 角色 |
| --- | --- |
| 剑盾 | Footman、Soldier、Knight、Templar、Prince、Duke、Count |
| 双剑 | Shinobi、Elite、IronMask |
| 双手剑 | Warrior、HeavyKnight、DarkKnight、Executioner、Immortal |

职业与武器是本次的初始搭配，可按游戏设计调整。法师、学徒、工程师和平民未作为持械近战敌人加入。

## 配置结构

`BP_Enemy_* → DA_CharacterConfig_* → Shared/<动作类型>/`

每套共用资源包含动画蓝图、移动 BlendSpace、两段攻击 Montage 与动作数据、攻击动作集、受击和死亡 Montage 与动作数据。模型、原始动画、材质与武器直接引用 `/Game/ModularRPGHeroesPolyart`，没有重复复制源美术资产。

初始技能与属性初始化 GE 复制自 `DA_PawnData_Enemy`，继续引用原有通用能力与 GE。每个敌人有独立 CharacterConfig；同类敌人共用动作集。若要让某只怪物拥有独立伤害或动作，先复制该动作集及相应动作数据，再改它的 CharacterConfig 引用。若要独立属性，也应复制初始化 GE 后单独配置。

没有增加闪避能力：参考敌人的 `DodgeActionData` 原本为空。

## 使用与后续调整

- 将对应 `BP_Enemy_*` 拖入现有关卡，或者通过 SpawnActor 生成。Auto Possess AI 设置为 Placed in World or Spawned，沿用原敌人的 AIController。寻路仍依赖关卡原有的导航配置。
- 胶囊初始半径 42、高度半值 96；模型偏移 Z=-96，Yaw=-90。血条初始高度为 125，可根据角色头盔和镜头调整。
- 武器组件名为 `WeaponRight`、`WeaponLeft`，使用素材包的 `RightWeaponShield`、`LeftWeaponShield` 挂点，关闭武器碰撞和导航影响。伤害仍由原通用能力的范围检测执行。
- 攻击 Montage 保留参考敌人的攻击事件、连招窗口和移动限制通知，并按新动画长度同比缩放时间。这是可运行的初始配置，**命中帧、前后摇、武器尺寸和连招手感需要在实际战斗中调整**。
- 同类角色的动作蓝图共享；动画包没有对应 Boris 的专用起跑/停跑片段，因此相关节点使用该持械类型的 Run 动画作为初始替代。

本次只进行资产重新加载、骨骼与引用、通知存在性及生成组件挂点等必要检查，不进行完整 PIE 战斗、联网或平衡验证。未修改现有关卡、原 Boris 角色、原 PawnData 或素材包源文件。

生成清单与检查记录位于 `Saved/EnemySetup`。该目录中的脚本用于本次配置，不要在手动调整新资产后直接重跑生成脚本，以免覆盖自己的调整。临时编辑器工具也放在该目录，只负责写入 UE Python 未暴露的构造节点挂点，不是游戏运行依赖。
