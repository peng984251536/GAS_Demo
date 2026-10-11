# 统一伤害与治疗 API

技能在服务器确认目标后调用 UCC_DamageStatics::ApplyDamage(SourceASC, TargetASC, Request)。
蓝图搜索 Apply Damage（GAS|Damage），构造 CC Damage Request；不再单独调用飘字 API。
返回 Applied 只表示 GE 接受应用，免疫/零伤害仍可能没有扣血。

默认 Request：SkillScale=1、FlatDamage=0、DistanceMultiplier=1、WeakPointMultiplier=1、CriticalPolicy=Random。
固定伤害：SkillScale=0，FlatDamage 填正数。弱点倍率不代表暴击。
Forced 暴击只用于服务器已确认的判定，不能信任客户端随意传入。

```cpp
#include "Damage/CC_DamageStatics.h"
FCC_DamageRequest Request;
Request.SkillScale = 2.f;
Request.FlatDamage = 20.f;
Request.CriticalPolicy = ECC_CriticalPolicy::Random;
Request.bHasHitResult = true;
Request.HitResult = Hit;
UCC_DamageStatics::ApplyDamage(SourceASC, TargetASC, Request);
```

治疗调用 ApplyHealing(SourceASC, TargetASC, 正治疗量)，自动限制到缺失生命并显示实际治疗数字。

计算顺序：

```text
((AttackPower * SkillScale + FlatDamage)
 * max(0, 1 + OutgoingDamageBonus)
 * DistanceMultiplier * WeakPointMultiplier * CriticalFactor - Defense)
 -> max(0, 结果) * IncomingDamageMultiplier
```

新增属性：Defense=0、OutgoingDamageBonus=0、IncomingDamageMultiplier=1、CriticalChance=0、CriticalMultiplier=1.5。
AttackPower/增伤/暴击属性创建 Spec 时快照，Defense/承伤倍率执行时读取。
Buff 通过 GE 修改属性；不要在技能中再次收集 Buff 重算。
多个减伤 Buff 的叠加方式必须按 GAS 聚合规则明确配置，不能假设每个 MultiplyAdditive 修正独立连乘。

Instant 伤害 GE 输出正 IncomingDamage，AttributeSet 消费并清零，完成 Health 修改。
IncomingDamage/IncomingHealing 不复制，也不得配置为持续 Modifier。
Status.DamageImmunity 阻止普通伤害。普通攻击不能直接修改 Health；初始化、复活、资源重置仍可使用专用流程。

AttributeSet.OnDamageResolved 和项目 ASC.OnDamageResolved 是服务器本地 C++ 委托。
结果区分 ResolvedDamage（攻防计算值）和 HealthDamage（实际生命变化），含明确暴击/治疗标记与来源目标。
默认飘字显示 ResolvedDamage，残血5受到100会显示100。
ASC 自动创建的 CC_CombatFeedbackComponent 上 bShowActualHealthDamage=true 可显示实际掉血。
ShieldAbsorbed 目前是预留字段，护盾与元素抗性没有在本次实现中加入。

伤害数字走独立 GameplayCue.Combat.DamageText 原生 Cue，项目 CC_GameplayCueManager 自动注册并在编辑器重新扫描后恢复，无需新建 Cue 蓝图。
更换 Globals 配置后需关闭并重启编辑器，Live Coding 不足以更新反射类型和 Globals 实例。
原 HitImpact 只播放声音和 Niagara；技能要显示命中特效仍需正常触发自己的命中 Cue。
数字默认显示给本地攻击者和本地受伤者，分屏按玩家隔离；DamageTextSubsystem.bShowAllDamage 可显示所有数字。
默认逐击显示（MergeWindow=0）；开启累计后按玩家、目标、来源、类型和样式分组，不合并伤害与治疗。
数字使用本次结算的位置副本，默认包围盒顶部+20cm；Request.bUseHitLocation=true 时使用 HitResult.ImpactPoint。

删除的旧 API：ApplyGameplayEffectSpecByTag、ApplyHeavyAttackDamage、ApplyDamageWithText。
独立表现工具 UCC_DamageTextStatics 已恢复，保留 ShowDamageText 与 ShowDamageTextFromDelta。
蓝图旧节点需手动替换，旧 GE 若仅用负 SetByCaller 直接扣 Health，应改为新 API 内置的 CC_GE_LyraStyleDamage。
不要在新 GE 上额外添加 Health 扣减 Modifier，也不要让旧 Cue 再显示数字。

投射物若需要发射快照：发射时创建 CC_GE_LyraStyleDamage Spec，填写 SetByCaller 与 Context；每个目标使用独立 Context，在服务器命中时应用。
ApplyDamage 是即时结算便捷入口，它在调用时创建快照。周期伤害可创建该 GE 的子类配置 Period，每个执行会独立产生结果；默认 Context 禁用暴击，需暴击时显式设置策略。
无 ASC 的环境伤害第一期需配置服务器环境来源 ASC。元素真实伤害、完整 DamageDefinition 数据资产、专用伤害日志属于后续扩展。

死亡仍由现有 CC_ListenToHealth 监听 Health 触发；结算系统不重复激活死亡能力。

验证入口：Tests/Standalone/RunDamageCalculatorTests.cmd；UE 自动化筛选 GAS_Demo.Damage 与 GAS_Demo.UI。
多人 PIE 还需检查来源归属、监听服务器不重复数字、周期伤害复制和分屏。GameplayCue 是短暂表现，不能用作可靠战斗账本。

本机 UE 配置同时兼容旧 AbilitySystemGlobals 节和 GameplayAbilitiesDeveloperSettings 节。

当前已发现需手动迁移的蓝图：
- /Game/GAS_Demo/AbilitySystem/Abilities/CommonAbilities/BP_GA_Attack
- /Game/GAS_Demo/AbilitySystem/Abilities/PlayerAbilities/GA_Attack

将旧节点的 Source/Target ASC 接到 ApplyDamage；若原 Damage 是固定伤害，Request.SkillScale=0、FlatDamage=原 Damage 正值，不再传负号或任意 SetByCaller Tag。

## 本次验证结果（2026-10-09）

- UE 5.6 GAS_DemoEditor Win64 Development 的 UHT、C++ 编译和链接通过。
- 独立纯计算器测试通过，覆盖增伤、减伤、防御、零伤害、非法值和溢出。
- GAS_Demo.Damage.LyraStyle.BuffsSnapshotAndHealth 通过：真实 Buff 聚合、攻击快照、目标实时减伤、明确暴击、残血实际扣血、免疫和治疗。
- GAS_Demo.Damage.FeedbackAndGrouping 通过：服务器结算自动发送一次原生 Cue、默认逐击显示、按目标/玩家/样式分组、Context 元数据序列化与 HitResult 深复制、同步 Health 回调内治疗重入。
- GAS_Demo.Abilities.HealthListener.ParentInitialization 与 GASDemo.UI.HealthBars.RegistrationAndGAS 回归通过。
- 差异空白检查通过；未修改或保存任何 .uasset。

构建日志：Saved/BuildLogs/DamageRefactorBuild.log。
自动化日志：Saved/BuildLogs/DamageRefactorTests.log。
自动化报告：Saved/Automation/DamageRefactor/index.json。

启动编辑器会报告 BP_GA_Attack 和 GA_Attack 仍引用已删除旧节点，这是本次约定由用户迁移的部分。请关闭并重启编辑器，用新的 ApplyDamage 替换这些节点；不要仅依赖 Live Coding。
尚未运行多客户端 PIE/真实网络测试；本次 Context 序列化测试没有覆盖网络 PackageMap 中的 Actor 引用与网络相关性。护盾、元素抗性、完整技能伤害定义数据资产属于后续扩展，当前 ShieldAbsorbed 仅为预留字段。

## 蓝图快速接入

1. 完整重启编辑器，使新 Globals、Cue 管理器和反射类型生效。
2. 在服务器确认命中的技能流程中，获取攻击者和目标的 AbilitySystemComponent。
3. 创建 Make CC Damage Request，或拆分 ApplyDamage 的 Request 引脚。
4. 选择属于 CC_DamageStatics、分类 GAS|Damage 的 Apply Damage，接入两方 ASC 和 Request。
5. 同一次命中不再额外应用旧扣血 GE，也不再调用数字 API。

最小测试：SkillScale=0、FlatDamage=100、DistanceMultiplier=1、WeakPointMultiplier=1、CriticalPolicy=Disabled。其余位置选项保持默认即可。
这个测试在无攻防修正时产生100伤害；固定伤害仍会受到增减伤和防御影响。
ApplyDamage 不会自动发送服务器 RPC；如果在客户端技能分支调用，它返回 NotAuthority。应使用已有的服务器技能/命中确认流程。

技能伤害例：SkillScale=2、FlatDamage=20 表示 200% 攻击力加20。攻击力100、无其他修正时伤害220。
DistanceMultiplier 是已经算好的距离系数，不会自动测距；WeakPointMultiplier 是部位倍率，不自动识别头部骨骼。
DamageType 目前用于分类和数字合并，不会自动增加元素抗性或切换字体颜色。

## 用 GE 配置 Buff

Buff 使用 Duration/Infinite GameplayEffect 修改属性，伤害 API 会自动读取聚合结果。

| 想实现的效果 | 修改对象 | GE Attribute | 简单配置示例 |
|---|---|---|---|
| 攻击力增加10 | 攻击者 | AttackPower | Additive +10 |
| 增伤50% | 攻击者 | OutgoingDamageBonus | Additive +0.5 |
| 减伤20% | 受击者 | IncomingDamageMultiplier | 默认基础1，Additive -0.2，得到0.8 |
| 防御增加30 | 受击者 | Defense | Additive +30 |
| 暴击率25% | 攻击者 | CriticalChance | 初始化为0.25，或对原暴击率增加0.25 |
| 暴击伤害2倍 | 攻击者 | CriticalMultiplier | 初始化/专用配置为2，默认是1.5 |

以上减伤例采用加法叠加口径；独立乘算减伤需要另行明确聚合规则。
不要先在蓝图将伤害乘增伤/减伤一次，又让 Execution 再乘一次。
例如固定伤害100、增伤50%、减伤20%、防御0、未暴击：最终为100*1.5*0.8=120。

## 暴击、治疗与位置

CriticalPolicy：Disabled=不暴击；Random=服务器按 CriticalChance 随机；Forced=服务器确认的必定暴击。
默认 CriticalChance=0，因此 Random 在未配置属性时不会暴击。CriticalMultiplier 表示总倍率，不是额外增加的比例。
治疗使用 ApplyHealing(治疗者ASC, 目标ASC, 正数Amount)，数字显示实际恢复量；不要传负伤害模拟治疗。
默认数字从目标包围盒顶部上方20cm出现；要从命中点出现，同时填 HitResult、bHasHitResult=true、bUseHitLocation=true。
位置在结算时确定，数字随后不会持续跟随目标移动。

## 阅读代码的顺序

1. Public/Damage/CC_DamageStatics.h：技能调用入口及输入/返回约定。
2. Public/Damage/CC_DamageTypes.h：Request 各字段与 Resolved 结果的含义。
3. Private/Damage/CC_DamageExecution.cpp：项目唯一伤害 Execution，包含属性捕获、暴击与伤害输出。
4. Public/Damage/StandaloneDamageCalculator.h：普通 C++ 纯公式，不创建 UObject。
5. Private/Attribute/CC_AttributeSet.cpp：中转值消费、实际扣血与结算通知。
6. Private/Damage/CC_CombatFeedbackComponent.cpp：结果数值选择及数字 Cue。
7. Private/GameplayCues/CC_GameplayCue_DamageText.cpp：客户端观看者筛选。
8. Private/UI/DamageText/CC_DamageTextSubsystem.cpp：条目分组、排列和回收。

这些类、关键函数与参数已补充中文注释；头文件注释也为蓝图节点/结构体字段提供说明。


## GE 定义文件位置

伤害和治疗 GE 已分别拆成独立文件：
- Public/Effects/Damage/CC_GE_LyraStyleDamage.h 与 Private/Effects/Damage/CC_GE_LyraStyleDamage.cpp。
- Public/Effects/Damage/CC_GE_Healing.h 与 Private/Effects/Damage/CC_GE_Healing.cpp。

Damage/CC_DamageExecution.h/.cpp 负责项目唯一的伤害 Execution。
GE 类名和 /Script/GAS_Demo 中的反射身份不变，已有父类引用不因源码目录调整而改变。


## 独立显示数字（2026-10-10 恢复）

UCC_DamageTextStatics 保留在 UI/DamageText，负责只显示数字，不扣血、不应用 GE，也不播放 HitImpact 特效。
ShowDamageText(TargetASC, 123.f) 可独立显示普通数字；治疗使用 Heal 样式。
ShowDamageTextFromDelta(TargetASC, HealthBefore, HealthAfter, bCritical) 根据已观测到的生命差值显示。
两者均可选传 SourceASC，提供时按正常来源/目标规则筛选；省略来源时显示给收到该 Cue 的本地玩家。
服务器调用使用数字 Cue 复制；客户端调用只执行本地表现。
常规 ApplyDamage/ApplyHealing 仍自动显示数字，同一次伤害不要同时再调用本工具。


## 飘字显示链路简化

已移除飘字 Controller 和 Model：
`数字 Cue → CC_DamageTextSubsystem → SCC_DamageTextLayer`。
CC_DamageTextWidget 只负责创建画布、样式配置、订阅更新和释放绑定。
Subsystem 是活动数字的唯一数据来源，Slate 按当前玩家只读筛选，不再每帧复制列表。
普通/暴击/治疗的颜色、字号和数字格式集中在 OnPaint，删除单独的格式/字号/颜色函数，以及 Widget 的三个样式 getter。
RootLayout 切图时释放 Slate，重建时重新绑定；玩家/世界不匹配时不绘制旧数据。
UCC_DamageTextStatics 的独立显示入口继续保留；本次不改变伤害结算或自动反馈机制。
本次按用户要求仅做源码引用、数据流和生命周期静态检查，没有启动编译或自动化测试。


## 伤害计算层简化

UGASDamageExecutionBase 和 UCC_LyraStyleDamageExecution 已合并为 UCC_DamageExecution，旧四个源文件已移除。
新类直接继承 UGameplayEffectExecutionCalculation，只有构造函数和 Execute_Implementation 两个函数。
不再使用 ConfigureAttributes、PrepareDamageInput、动态字段绑定和配置状态变量。
SetByCaller/免疫标签声明统一放在 CC_DamageTypes.h，原生注册名称与现有值不变。

当前主链路：
`ApplyDamage → Effects/Damage 的伤害 GE → UCC_DamageExecution → AttributeSet 消费 IncomingDamage`。
StandaloneDamageCalculator 保留纯数学函数用于公式阅读和独立测试，不属于另一套运行时伤害系统。
攻击方快照、目标实时防御/减伤、明确暴击、完整伤害与实际扣血分离的规则继续沿用。
CoreRedirects 将两个旧 Execution 的资产引用迁移到新类；GE 类名和统一 API 不变。
本次只进行引用、捕获配置、输出路径和公式一致性的源码静态检查，未运行编译、自动化或 PIE。
