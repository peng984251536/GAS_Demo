# Buff 系统基础设施现状与待办

基于 `ui/framework-review-fixes` 分支（提交 `ea85261`）整理。状态变化时请更新本文件。

状态：✅ 已具备　🟡 部分具备　❌ 缺失

## 已具备

| 能力 | 状态 | 位置 |
|---|---|---|
| Buff 定义继承 GE（持续时间、叠层、标签条件、等级） | ✅ | `UCC_BuffDefinition` |
| 每个 GE 句柄一份 Behavior 实例，对称的 OnApplied / OnRemoved | ✅ | `UCC_BuffBehavior`，`UCC_BuffComponent::StartSpecials/StopSpecials` |
| 抑制时停止、恢复时重启特殊效果 | ✅ | `UCC_BuffComponent::RefreshFromGAS` |
| Buff 列表复制到客户端 | ✅ | `UCC_BuffComponent::ActiveBuffs` |
| 事件触发 GA（AbilityTriggers） | ✅ | `CC_FindPlayerTarget` 等已在使用 |
| 伤害 Execution | ✅ | `UCC_DamageExecution` |
| 统一伤害入口 | 🟡 | `UCC_DamageStatics::ApplyDamage` 存在，但箭绕过了它 |

## 待办（按依赖顺序）

### 1. 阵营系统 🟡

- ✅ `UCC_TeamStatics`（`IsHostile` / `GetEffectiveTeam` / `GetBaseTeam`），基础阵营来自 `CharacterConfig.Team`，为空时用角色类默认值（玩家 `CCTags.Team.Player`、敌人 `CCTags.Team.Enemy`）。
- ✅ 魅惑：`CCTags.Status.Charmed` 生效时，有效阵营 = 施加该 GE 的 Instigator 的基础阵营（只在服务器可靠）。
- ✅ AI 选目标（`ACC_AIController`）改用 `IsHostile`，详见 `AI-Targeting.zh-CN.md`。
- ❌ `UCC_Primary::HitBoxOverlapText`（目前命中所有 Pawn，敌人会误伤同伴）。
- ❌ `ACC_ArrowProjectile::OnArrowHit`（目前只排除射手）。
- `CC_FindRangedTarget` / `CC_FindPlayerTarget` 是旧索敌方案，行为树已不再激活，用 `harness:allow R4` 标注保留。
- 解锁：魅惑、反弹、召唤物敌我判断（近战和箭接入后完整生效）。

### 2. 伤害管线统一 🟡

- `ACC_ArrowProjectile::OnArrowHit` 改为调用 `UCC_DamageStatics::ApplyDamage`。
- `FCC_DamageRequest` 增加 `SourceTags`（`Damage.Source.*`）。
- 扣血后发送 `Event.Combat.DamageDealt` / `DamageTaken`。
- 解锁：所有"命中/伤害后"类 Buff。

### 3. 通用 Behavior：ApplyEffectOnDamage ❌

- 依赖 2。完成后冻结、魅惑、中毒、减速等只需配置 GE。

### 4. 放箭流程拆分 ❌

- `UCC_BowShoot::ReleaseArrow` 拆为 Build → Modify → Resolve → Execute。
- 新增 `FCC_ProjectileFireContext`、`FCC_ArrowPayload`，`UCC_BuffBehavior::ModifyProjectileFire`，`UCC_BuffComponent::ModifyProjectileFire`。
- 解锁：分裂箭、爆炸箭、穿透箭等。

### 5. 动作集按标签切换 ❌

- `CharacterConfig.AttackActionSetOverrides`（有序）替代 `UCC_BuffDefinition::AttackActionSetOverride`。
- `UCombatActionData.bAllowMovement` + `State.MovementLocked`。
- 删除 `UCC_BuffComponent::GetAttackActionSetOverride`。

### 6. 遗留字段迁移 ❌

- `SummonClass` / `SummonCount` / `SummonRadius` / `bDestroySummonsOnRemoval` → `UCC_BuffBehavior_Summon`。
- 完成 5、6 后，`UCC_BuffDefinition` 只剩显示信息与生命周期设置。

### 7. 反弹投射物 ❌

- 依赖 1。箭增加 `Deflect`；近战判定在有 `Ability.Deflect.Projectile` 时检测投射物。
