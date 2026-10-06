# 伤害飘字系统

自定义 Slate 批量绘制 · GameplayCue 触发 · 连击合并 · 区分暴击/治疗。

---

## 一、为什么选这个方案

飘字有三条常见路线，性能差距很大：

| 方案 | 同屏 20 个数字的开销 | 评价 |
|---|---|---|
| 每个数字一个 WidgetComponent | 20 个 Slate 控件 + 20 张渲染目标纹理 + 20 次 draw call + 20 套 UMG 布局计算 | 高频命中直接掉帧 |
| 单个 UMG 控件 + 池化 TextBlock | 20 个子控件 + 1 套布局计算 | 可用，但仍受 UMG 布局拖累 |
| **本方案：单 Slate 控件批量绘制** | **1 个控件 + 1 次绘制** | 数量增加接近零额外固定成本 |

关键差别在于：**本方案不创建子控件**。飘字是纯数据（`FCC_DamageTextEntry`），
绘制在 `SCC_DamageTextLayer::OnPaint` 里一次循环画完。飘字从 1 个涨到 50 个，
控件数量始终是 1，布局计算次数始终是 1。

另外两处优化：

**每帧只构建一次投影矩阵**（`FSceneViewProjectionData`），整批飘字复用。
如果每个飘字各自调一次 `ProjectWorldToScreen`，那部分重复开销会随数量线性增长。

**空闲期完全不 Tick**。`UCC_DamageTextSubsystem::IsTickable()` 在没有活动飘字时返回
`false`，战斗结束后的整个帧循环里这套系统不参与任何计算。

---

## 二、文件与职责

| 文件 | 职责 |
|---|---|
| `Public/UI/DamageText/CC_DamageTextTypes.h` | 飘字结构体与样式枚举。纯数据，不含 Slate/UMG 对象 |
| `Public/UI/DamageText/CC_DamageTextSubsystem.h` + `.cpp` | 活动列表、连击合并、老化回收。不做绘制 |
| `Public/UI/DamageText/SCC_DamageTextLayer.h` + `.cpp` | Slate 绘制层。批量投影 + 批量绘制 |
| `Public/UI/DamageText/CC_DamageTextWidget.h` + `.cpp` | UMG 包装，让绘制层能拖进 HUD |
| `Public/UI/DamageText/CC_DamageTextStatics.h` + `.cpp` | 技能侧调用入口：应用伤害 + 弹飘字 |

**Build.cs 不需要改。** `Slate`、`SlateCore`、`UMG`、`GameplayAbilities` 已在
`PublicDependencyModuleNames` 中。

---

## 三、接入步骤

### 1. 编译 C++

新增 5 个源文件，其中 3 个是 UObject/UStruct（有 `.generated.h`），需要重新生成项目文件。

### 2. 绘制层（已由 UI 框架托管）

绘制层现在由 `CC_RootLayout` 的**世界覆盖层**自动创建（根布局默认值 `Damage Text Layer Class`），
位于所有页面层之下、不参与输入，随根布局一起挂载、切图和分屏；打开菜单时默认隐藏。

- **不需要**再往 HUD 蓝图里拖 Damage Text Widget。若之前拖过，请删除，否则飘字会画两遍（运行时会输出 Warning）。
- 设为空即不创建飘字层。字号与上升高度目前使用 `CC_DamageTextWidget` 的 C++ 默认值（普通 28、暴击 40、上升 80）。

这个控件不需要输入、不抢焦点，所以**不要**给它换成 `CommonUI` 的 ActivatableWidget。

### 3. 技能侧改用新的应用伤害入口

原来直接调用：

```cpp
SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
```

改成：

```cpp
UCC_DamageTextStatics::ApplyDamageWithText(
    SourceASC, TargetASC, DamageEffectClass,
    /*SkillScale*/ 2.0f,
    /*FlatDamage*/ 20.0f,
    /*HitMultiplier*/ 1.0f);
```

它会做三件事：填入 SetByCaller 参数、应用 GE、发一条带伤害值的 GameplayCue。
伤害公式与 `UGASDamageExecutionBase` 完全一致（读取攻击方当前聚合的 AttackPower），
所以飘字显示的数值和实际结算用同一套输入。

`HitMultiplier > 1` 会被标记为**暴击**飘字。当前项目没有独立的暴击属性，
所以用倍率表达；如果你的暴击走别的方式，改用下面的 `ShowDamageText` 自行指定样式。

### 4. 在 GameplayCue 蓝图里确认开关

`UCC_GameplayCue_HitImpact` 新增了 `bShowDamageText`，默认开启。
如果你有只放特效、不出数字的 Cue，可以关掉它。

---

## 四、四种调用方式

按场景选：

**`ApplyDamageWithText`** — 推荐。应用伤害 GE 并弹飘字，一次调用完成。
适合绝大多数技能。

**`ShowDamageText`** — 只弹表现，不应用 GE。
适合伤害已经由别处结算的情况（环境伤害、脚本事件、你自己的扣血流程）。

**`ShowDamageTextFromDelta`** — 传入扣血前后的生命值，用差值弹字。
**这是唯一能显示"实际扣血值"的做法**，见下一节。

**`UCC_DamageTextSubsystem::ReportHit`** — 最底层，直接给世界坐标和数值。
适合非 GAS 来源的飘字（比如拾取、经验）。

---

## 五、关于"理论伤害"与"实际扣血值"的取舍

**默认行为：飘字显示理论伤害。**

原因在 `Docs/LyraStyleDamage.md` 里写过：Execution 会先把输出裁剪到目标当前生命值。
100 点伤害打在只剩 5 血的目标上，实际只扣 5，但**这个裁剪发生在 GAS 内部**，
外部在应用 GE 之前无法预知。

所以 `ApplyDamageWithText` 传的是理论值（`AttackPower * SkillScale + FlatDamage`，再乘 `HitMultiplier`）。
好处是数值和表现一次传递、天然复制到所有客户端、不需要额外的伤害数值 RPC。

代价是残血收割时飘字会显示 `100` 而不是 `5`。

**如果你要显示实际扣血值**，唯一可靠的做法是差值法：

```cpp
// 在权威端，应用 GE 前后各读一次生命值
const float Before = TargetASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute());

SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

const float After = TargetASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute());

UCC_DamageTextStatics::ShowDamageTextFromDelta(
    TargetASC, Before, After, /*bCritical*/ HitMultiplier > 1.0f);
```

注意这条路要**同时**去掉 `ApplyDamageWithText` 里那次发 Cue 的调用，否则会飘两次。
做法是把 `ApplyDamageWithText` 的飘字开关关掉，或者干脆不用它，
自己填 Spec 参数后走上面的差值流程。

这套差值法只应在权威端调用一次；表现靠 GameplayCue 复制到客户端。

---

## 六、连击合并

同一目标在 **0.25 秒**内的连续命中会累加到同一个数字上：数值相加、计时重置、
触发一次弹跳放大。连击里出现暴击，整个数字会**升格**成暴击表现（橙字大字）。

合并判定要求**同时**满足两个条件：在时间窗口内，且世界距离小于 `MergeDistance`（默认 120cm）。
第二个条件是为了区分画面里两个不同的敌人——只按时间判断会把它们的数字错误地加在一起。

在 `UCC_DamageTextSubsystem` 上可以调：

| 参数 | 默认 | 说明 |
|---|---|---|
| `MergeWindow` | 0.25 | 合并时间窗口（秒）。设为 0 关闭合并 |
| `MergeDistance` | 120 | 合并距离阈值（厘米） |
| `Lifetime` | 1.1 | 飘字存活时长（秒） |

**为什么合并是性能上的关键一招**：它把"玩家感知到的一次伤害"和"实际飘出的数字数量"
解耦了。快速连击下同屏数字数量可能降一个数量级，而表现反而更清晰。

---

## 七、编译状态与已知问题

**当前状态：已修正两处编译错误，尚未重新编译验证。**

第一次编译（2026-09-18）只报了两个错误，两个文件各卡在第一个错误上中止，
修掉后应当可以通过。记录如下，避免以后再踩：

**已修正 1 — `ApplyGameplayEffectSpecToTarget` 不返回 bool。**
它返回 `FActiveGameplayEffectHandle`。原来的 `const bool bApplied = ...` 报 C2440。
正确写法：

```cpp
const FActiveGameplayEffectHandle AppliedHandle =
    SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
const bool bApplied = AppliedHandle.WasSuccessfullyApplied();
```

**已修正 2 — `ULocalPlayer::GetProjectionData` 的参数顺序。**
真实签名是

```cpp
bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData,
                       int32 StereoViewIndex = INDEX_NONE) const;
```

出参在**第二位**。原来按 `(Viewport, eSSP_FULL, OutData)` 写会报 C2664，
因为 `EStereoscopicPass` 转换不到 `FSceneViewProjectionData&`。
第三个参数省略即用默认的 `INDEX_NONE`（非立体渲染）。

**仍未验证的风险点**（这几处没报错，但也没实际跑过）：

**`FSceneView::ProjectWorldToScreen` 的重载。** 走的是模板 `T& out_ScreenPos`，
传 `FVector2D` 应该匹配。若报错，改用 `FVector4` 再自己取 XY。

**`FGameplayCueParameters::AggregatedSourceTags` 的传递。**
用于传暴击/治疗标记。如果该字段在 Cue 传递过程中被清空，
改用 `EffectContext` 的 `AddSourceTag`，或者扩展 `RawMagnitude` 的正负约定。

**`FSlateFontInfo` 用的默认字体。** `FCoreStyle::GetDefaultFontStyle("Bold", Size)`。
飘字只显示数字和 `+` 号，默认字体足够；如果以后要在飘字里加中文，需要换成项目的中文字体资产。

**建议首次编译通过后立刻做的最小验证**：在任意技能里调一次
`UCC_DamageTextStatics::ShowDamageText(TargetASC, 123.0f)`，
确认能飘出 `123`。这能筛掉绝大部分接入问题，再往下接完整伤害流程。

---

## 七之二、编译与查错

用 `Scripts\BuildAndLogErrors.cmd` 编译。它会把完整日志写到
`Saved\BuildLogs\Build.log`，并把 `error` 行单独提取到
`Saved\BuildLogs\BuildErrors.txt`（通常只有几行）。

要找人帮忙看报错时，直接贴 `BuildErrors.txt` 的内容即可，不用翻完整日志。

如果你的引擎不在 `D:\GameTools\UE_5.6`，改脚本顶部的 `EngineRoot`。

---

## 八、性能观测建议

想确认这套方案的实际收益，可以看两个数：

**同屏飘字数量**。在 `UCC_DamageTextSubsystem` 上加一个非 Shipping 的屏幕调试输出
`ActiveEntries.Num()`，对着人堆放 AoE 技能观察。正常情况下几十个飘字对帧率应该没有可测影响。

**`stat Slate`**。观察 draw call 数量。本方案无论多少飘字，绘制调用应该只有个位数增长
（描边让每个飘字多 4 次 MakeText，但它们会合并进同一批 draw call）。

如果发现飘字数量异常高，先看 `MergeWindow` 是不是被调太小了。

---

## 参考

- 伤害公式与 GAS 接入：[LyraStyleDamage.md](LyraStyleDamage.md)
- UI 框架分层：[UIArchitecture.zh-CN.md](UIArchitecture.zh-CN.md)
- [FSceneViewProjectionData API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSceneViewProjectionData)
- [ULocalPlayer::GetProjectionData](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Engine/ULocalPlayer/GetProjectionData?application_version=5.1)
- [TArray::RemoveAtSwap (EAllowShrinking)](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/TArray/RemoveAtSwap?application_version=5.6)
