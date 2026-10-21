# GAS_Demo 工作约定

UE5 + GAS 的动作战斗 Demo。C++ 源码在 `Source/GAS_Demo`，设计文档在 `Docs/`（中文）。

## 修改 Buff / 战斗机制前必读

- 设计原则与分层：`Docs/BuffSystem.zh-CN.md`
- 目标架构、类关系与时序图：`Docs/BuffSystem-Architecture.zh-CN.md`
- AI 寻敌与阵营：`Docs/AI-Targeting.zh-CN.md`
- AI 群体战斗（个体自由交战 + 攻击者登记表 + Detour Crowd，编队接口预留）：设计与接入 `Docs/AI-Combat.zh-CN.md`（取代旧的 `AI-Engagement*`）。新增攻击能力必须授予 `CCTags.CCAbilities.Attack`（攻击期间切 Crowd ObstacleOnly）。
- 案例手册（分裂箭、爆炸箭、冻结、魅惑、反弹等）：`Docs/BuffSystem-Cookbook.zh-CN.md`
- 基础设施现状与待办：`Docs/BuffSystem-Roadmap.zh-CN.md`
- Buff 三选一（获取途径）：`Docs/BuffSelection.zh-CN.md`
- 选点召唤随从（地面高亮选点、召唤物继承阵营 `SetTeamOverride`）：`Docs/SummonAlly.zh-CN.md`
- 生存模式（波次刷怪、共享经验、倒计时复活、结算）：设计 `Docs/SurvivalMode.zh-CN.md`（第 0 节为实现偏差），接入与调参 `Docs/SurvivalMode-Usage.zh-CN.md`。死亡判定统一走 `UCC_AttributeSet::OnOutOfHealth` → `UCC_GameEventSubsystem::OnCharacterDied`，不要再在 Health 变化回调里判定死亡。
- 新增或修改 Buff 时，按 `.claude/skills/add-buff/SKILL.md` 的流程执行。

## 硬性规则（违反即视为错误）

1. **Buff 只声明状态和规则，机制由系统实现。** 技能、AI、伤害代码里不得出现针对某个具体 Buff 的判断（如 `HasBuff(分裂箭)`）。系统只认标签、属性、上下文字段。
2. **`UCC_BuffDefinition` 不再新增玩法字段。** 它只保存显示信息和生命周期设置。新机制用标签（状态层）、授予 GA（能力层）或 `UCC_BuffBehavior` 子类（规则层）实现。现有的 `AttackActionSetOverride` 和 `Summon*` 字段是待迁移的遗留项。
3. **所有伤害必须经过 `UCC_DamageStatics::ApplyDamage`**，并填写来源标签（`Damage.Source.*`）。不得在别处直接构造伤害 GE 的 Spec，否则"造成伤害后"类 Buff 不会触发。
4. **敌我判断只能通过统一的敌对关系查询**（规划中的 `UCC_TeamStatics::IsHostile`）。不得新增基于 Actor Tag 写死的敌我判断。
5. **Buff 授予的一切必须能对称收回**：`OnApplied` 创建或订阅的内容，`OnRemoved` 里必须清理；被抑制、叠层、死亡、移除这几条路径都要覆盖。Behavior 基类的自动回收工具（`ListenGameplayEvent` / `StartTimer` / `SpawnOwnedActor` / `ApplyChildEffect`）实现后，子类只能通过它们创建资源。
6. **共享配置不存实例状态**：Definition、Behavior 模板、DataAsset 运行时只读。Behavior 的 `UPROPERTY` 要么是 `EditDefaultsOnly` 配置，要么是 `Transient` 运行时状态。
7. **钩子上下文的可修改字段必须注明合并方式**（`// 合并：相加 / 相乘 / 取最大 / OR`），适用于 `*HookContext`、`*FireContext`、`*Payload`。

## 改完之后

- 运行 `Tools/HarnessChecks/check_buff_rules.sh`，新增的报错必须处理或说明理由。确有理由的例外在行尾写 `// harness:allow R<编号> <理由>`。
- 该脚本已配置为 Claude Code 的 PostToolUse hook（`.claude/settings.json`），每次编辑 `Source/GAS_Demo` 后自动检查被改的文件。
- Buff 相关改动需补充或更新自动化测试（`Source/GAS_Demo/Private/Tests/`，命名 `GAS_Demo.Buff.*`）。
- 基础设施状态有变化时，同步更新 `Docs/BuffSystem-Roadmap.zh-CN.md`。
