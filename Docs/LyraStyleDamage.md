# 独立伤害计算与 GAS 接入

本功能参考本机 LyraWithUnLua 的“技能应用 GE → Execution 计算 → 输出 Health 扣减”流程，
是新增实现，不复制其角色、武器、团队或死亡系统。原有 CombatDamage::FModifier、
FCalculator、UCC_GE_Damage、角色和 AttributeSet 均未修改；原有攻击不会自动改走新链路。

## 文件与职责

| 文件 | 职责 |
| --- | --- |
| Source/GAS_Demo/Public/Damage/StandaloneDamageCalculator.h | 标准 C++ 纯数值计算，可不使用 UE/GAS 单独测试 |
| Source/GAS_Demo/Public/Damage/GASDamageExecutionBase.h + Private/Damage/GASDamageExecutionBase.cpp | 通用 GAS 桥接，捕获属性并输出扣血，不依赖具体 AttributeSet |
| Source/GAS_Demo/Public/Damage/CC_LyraStyleDamage.h + Private/Damage/CC_LyraStyleDamage.cpp | 当前项目属性绑定，以及可直接使用的 UCC_GE_LyraStyleDamage |
| Tests/Standalone/DamageCalculatorTests.cpp | 可独立运行的数值场景测试 |
| Source/GAS_Demo/Private/Tests/CC_LyraStyleDamageTests.cpp | 实际 ASC、GE、属性快照及扣血的 UE 自动化测试 |

当前项目绑定 Source 的 UCC_AttributeSet.AttackPower 与 Target 的 Health。
AttackPower 是已经包含 Buff 的攻击力数值，不是旧计算器中默认 1 的攻击倍率。
当前没有 Defense 属性，因此项目适配默认防御为 0。

## 公式与数据约定

```text
RawDamage           = AttackPower * SkillScale + FlatDamage
DamageBeforeDefense = RawDamage * DistanceMultiplier * HitMultiplier
DamageAfterDefense  = max(0, DamageBeforeDefense - Defense)
FinalDamage         = DamageAfterDefense * IncomingMultiplier
```

所有数值必须有限且非负。非法输入或中途溢出会返回失败，接入层记录警告并放弃扣血。
队伍或免疫规则可以令 bDamageAllowed=false，此时得到有效的 0 伤害。
本实现没有暴击随机过程；HitMultiplier 由服务器认可的本次命中结果提供。

计算器返回理论伤害和中间结果。GAS 接入层再按实际当前 Health 限制扣血量，
所以 5 血目标受到 100 理论伤害时输出 -5。此输出仍可能被 AttributeSet 回调拒绝/调整，
不能把它当成所有回调结束之后的实际掉血通知。

普通攻击/防御 Buff 由 GE 修改属性。不要把这些 GE 的 Modifiers 再收集到计算器中。
默认攻击属性在 MakeOutgoingSpec 时快照；防御（绑定后）在执行时求值。
飞行中的弹丸是否应该读取命中时攻击力，由项目决定；构造绑定时第四个参数 false 可关闭攻击快照。
同一 Spec 重复应用会复用其攻击快照，需要新的攻击快照时创建新 Spec。

## C++ 使用示例

下面是调用示例，不会自动插入你的技能。SourceASC、TargetASC 必须已正确初始化并注册对应属性集。
需要包含 AbilitySystemComponent.h 和 Damage/CC_LyraStyleDamage.h。

```cpp
// 在权威端确认命中后调用；不能信任客户端随意提交的倍率/目标。
if (!IsValid(SourceASC) || !IsValid(TargetASC)
    || !SourceASC->IsOwnerActorAuthoritative()
    || !TargetASC->IsOwnerActorAuthoritative())
{
    return;
}

FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
// 如果有确认后的命中结果，可执行 Context.AddHitResult(HitResult)。
FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
    UCC_GE_LyraStyleDamage::StaticClass(), 1.0f, Context);
if (!Spec.IsValid())
{
    return;
}

Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, 2.0f);
Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, 20.0f);
Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::DistanceMultiplier, 1.0f);
Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::HitMultiplier, 1.0f);
SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
```

攻击力 100 时，上例理论伤害为 220，生命值不够时裁剪扣血。
纯固定伤害技能可设 SkillScale=0、FlatDamage=所需正数伤害。
即使 SkillScale=0，当前接入仍要求 Source 注册攻击属性；无属性环境伤害可另做适配。

## 蓝图使用

1. 编译后，可创建父类为 CC_GE_LyraStyleDamage 的 GameplayEffect 蓝图；继承的 Instant 与 Execution 已配置。
2. 在服务器确认命中后，用 Source ASC 的 Make Outgoing Spec 创建该 GE 的 Spec。
3. 按需通过 Assign Tag Set By Caller Magnitude 填写下表中的参数。
4. Apply Gameplay Effect Spec To Target，目标为受击者的 ASC。

| SetByCaller Tag | 未填写时 | 含义 |
| --- | --- | --- |
| Data.StandaloneDamage.SkillScale | 1 | 攻击力系数 |
| Data.StandaloneDamage.FlatDamage | 0 | 技能额外固定伤害 |
| Data.StandaloneDamage.DistanceMultiplier | 1 | 本次距离系数 |
| Data.StandaloneDamage.HitMultiplier | 1 | 本次部位/暴击等已决定的综合倍率 |

这些 Tag 由新代码原生注册，不需要修改 DefaultGameplayTags.ini。
所有伤害输入都是正数，最终接入层统一取负数扣血。
现有 ApplyGameplayEffectSpecByTag 会把传入 Damage 取负，不适合原样用于这些新参数。
新的 GE 专门对应一次伤害结算，不要再添加同一次扣血的 Health Modifier 或重复 Execution。
Buff 叠层留给属性聚合；这里每次执行只结算一次命中，并显式禁止输出再乘 GE StackCount。

## 防御、免伤、团队与更复杂规则

- 防御：纯计算器已经支持固定防御。未来有 Defense 属性后，在新增适配类构造函数的
  ConfigureAttributes 第三个参数绑定其 getter；无需更改纯计算器或通用 GAS 桥接。
- 团队/无敌：默认不假定你的团队和免伤规则。项目子类可重写 PrepareDamageInput，
  利用 ExecutionParams 取得攻防 ASC 和 Spec/Context，返回 false，或设置 bDamageAllowed=false。
  这类判断应发生在服务器；GE 上的应用条件或 AttributeSet 的拒绝机制也可承担相应规则。
- 距离/部位：当前不会自动读武器曲线或 HitResult，可在技能中计算后通过 SetByCaller 传入，
  也可在 PrepareDamageInput 中通过项目接口计算。
- 承伤倍率：FInput.IncomingMultiplier 默认 1。若作为 Buff 属性，请在扩展的 Execution 构造函数中
  注册该属性的 CaptureDefinition，并在 PrepareDamageInput 中用 GAS 捕获求值后填写。
- 穿甲、护盾、伤害中转属性、死亡、飘字：当前不自动接入。Health 输出策略适合现有直接扣血方案；
  如果要改成 IncomingDamage 元属性，应另做输出适配并由 AttributeSet 消费，不可仅换输出属性就完成。

Execution 是共享对象，PrepareDamageInput 只能修改当次局部输入，不能把当前目标或伤害存进成员。

## 验证

在工程根目录运行 `Tests\Standalone\RunDamageCalculatorTests.cmd`。
脚本使用本机 VS 2022 Community，启用 /W4 /WX，输出在 Intermediate/LyraStyleDamageValidation。
迁移到其他机器时按安装位置调整测试脚本的 vcvars64.bat 路径。

场景覆盖固定计算顺序、已加 Buff 的攻击不重复加成、禁止伤害、高防御、零倍率、
纯固定伤害、非法数值和中途溢出。纯数值测试不代替 UE 内的网络/属性捕获/扣血集成验证。

本次验证结果：

- 标准 C++ 数值测试通过（/W4 /WX）。
- UE 5.6 临时验证工程 UHT、编译和链接通过；使用新增代码及原有 CC_AttributeSet 的副本。
- UE 自动化测试 `GAS_Demo.Damage.LyraStyle.BuffsSnapshotAndHealth` 通过，验证真实 Buff 聚合、
  攻击快照、Buff 移除、新 Spec、扣血上限、单次伤害不重复乘 GE 层数及非法输入拒绝。
- 原工程的 UHT 被现有 `Public/Utils/CC_BlueprintLibrary.h:69` 阻塞：
  蓝图反射无法识别 `CombatDamage::FModifier`。遵循“不修改原代码”，未处理此处。
  因此新增功能尚未在原工程实际技能链路中运行，也未验证多人联机行为。
- 已核对新增前 133 个源码、配置和文档文件的 SHA-256，原文件内容均未变化。

编译日志：`Intermediate/LyraStyleDamageValidation/HostCompile.log`。
自动化结果：`Intermediate/LyraStyleDamageValidation/AutomationReport/index.json`。
临时工程和测试产物均在 `Intermediate/LyraStyleDamageValidation` 下。
