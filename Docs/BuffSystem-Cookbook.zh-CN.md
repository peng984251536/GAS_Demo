# Buff 案例手册

每个案例都按同一格式：归类 → 实现 → 依赖的基础设施 → 注意事项。原则见 `BuffSystem.zh-CN.md`。代码为伪代码。

---

## 狂暴：换一套攻击动作

- **归类**：状态层。
- **实现**：Buff 授予 `State.Combat.Berserk`。`CharacterConfig` 用有序数组配置"标签 → 动作集"，排在前面的优先，都不匹配时用默认 `AttackActionSet`。

```cpp
// CharacterConfig
TArray<FCC_TaggedActionSet> AttackActionSetOverrides;   // { RequiredTag, ActionSet }

UCombatActionSet* ACC_BaseCharacter::GetEffectiveAttackActionSet() const
{
    for (Entry : Config->AttackActionSetOverrides)
        if (Entry.ActionSet && ASC->HasMatchingGameplayTag(Entry.RequiredTag))
            return Entry.ActionSet;
    return Config->AttackActionSet;
}
```

- **为什么不把动作集放在 Buff 里**：蒙太奇绑定骨骼，同一个 Buff 挂到不同角色身上需要不同动作；对应关系应该由角色自己维护。
- **注意**：连招中途切换时，旧动作在新动作集里找不到，`NextComboIndex` 归零，连招会重新开始。

---

## 分裂箭：一次射出多支

当前已有实际实现：原生 GE 配置 `UCC_GE_BuffSplitArrow` + `UCC_BuffBehavior_SplitArrow`，可在 UE 中以此 GE 为父类创建 `GE_Buff_SplitArrow` 蓝图。实际字段与合并约定以 [四种 Buff](BuffSystem/FourExamples.zh-CN.md) 和 `FCC_ProjectileFireContext` 为准。下方保留原设计伪代码。

- **归类**：规则层，事前修改型。
- **依赖**：`UCC_BowShoot::ReleaseArrow` 拆分为 Build → Modify → Resolve → Execute；`ModifyProjectileFire` 钩子。

```cpp
struct FCC_ProjectileFireContext
{
    // 只读
    ACC_BaseCharacter* Shooter; AActor* Target; FVector Origin; FVector AimDirection;
    // 可修改
    TSubclassOf<ACC_ArrowProjectile> ProjectileClass;
    float Speed;
    int32 ProjectileCount = 1;   // 合并：相加
    float SpreadAngle = 0;       // 合并：取最大
    FCC_ArrowPayload Payload;
};

void UCC_BuffBehavior_SplitArrow::ModifyProjectileFire(Ctx, Stacks)
{
    Ctx.ProjectileCount += ExtraArrows * Stacks;
    Ctx.SpreadAngle      = Max(Ctx.SpreadAngle, SpreadAngle);
    Ctx.Payload.Damage  *= DamagePerArrowRatio;
}
```

- **注意**：近距离时多支箭可能命中同一目标。用 `Payload.VolleyId` 让同一目标在一次齐射中只受一次伤害，或用单支伤害倍率平衡。

---

## 爆炸箭：命中时爆炸

- **归类**：规则层。放箭时写入 Payload（事前修改），命中时由箭执行。
- **依赖**：箭携带 `FCC_ArrowPayload`；`OnArrowHit` 执行 Payload。

```cpp
struct FCC_ArrowPayload
{
    float Damage;
    float ExplosionRadius = 0;        // 合并：取最大；0 表示不爆炸
    float ExplosionDamageRatio = 0;   // 合并：相加
    FGameplayTagContainer SourceTags;
    int32 VolleyId;
};

void UCC_BuffBehavior_ExplosiveArrow::ModifyProjectileFire(Ctx, Stacks)
{
    Ctx.Payload.ExplosionRadius       = Max(Ctx.Payload.ExplosionRadius, Radius);
    Ctx.Payload.ExplosionDamageRatio += DamageRatio * Stacks;
}

void ACC_ArrowProjectile::OnArrowHit(...)
{
    ApplyDamage(OtherActor, Payload.Damage);
    if (Payload.ExplosionRadius > 0)
        对范围内 IsHostile 的目标（排除直接命中者）施加 Payload.Damage * ExplosionDamageRatio;
    Destroy();
}
```

- **为什么在发射时写入**：见设计原则第 5 节"发射时快照"。
- **组合**：与分裂箭同时存在时，自动得到"多支会爆炸的箭"，两个 Behavior 互不知道对方。

---

## 移动射击：射击时可以移动

- **归类**：状态层（不需要钩子）。
- **实现**：
  1. `UCombatActionData` 增加 `bAllowMovement`。
  2. 制作"移动射击动作集"：上半身蒙太奇，`bAllowMovement = true`。
  3. Buff 授予 `State.Combat.MoveShoot`；弓兵 CharacterConfig 配置该标签对应移动射击动作集（同"狂暴"）。
  4. `UCC_BowShoot` 开始时只读 `OwnedAction->bAllowMovement`：为 false 时加 `State.MovementLocked`，技能结束时移除。
- **注意**：移动组件、AI 移动、KeepDistance 都要统一以 `State.MovementLocked` 为准。

---

## 攻击使敌人冻结

- **归类**：玩家身上是规则层（事后反应型）；敌人身上的冻结是状态层。属于"作用对象转移"。
- **依赖**：统一伤害入口 + 来源标签 + `Event.Combat.DamageDealt`。

```cpp
// 通用 Behavior：造成符合条件的伤害后，按概率给目标施加 GE
class UCC_BuffBehavior_ApplyEffectOnDamage : public UCC_BuffBehavior
{
    FGameplayTagQuery RequiredSource;          // 例如 Damage.Source.Melee，默认排除 DoT
    float Chance;
    TSubclassOf<UGameplayEffect> EffectToApply;
    // OnApplied 监听 Event.Combat.DamageDealt，OnRemoved 取消监听
};

GE_Freeze（UCC_BuffDefinition 蓝图）
  授予标签 State.Frozen；施加条件：目标没有 Immunity.Freeze
  Behavior：OnApplied 取消目标技能 + CustomTimeDilation = 0；OnRemoved 恢复
```

- 攻击类技能的 `ActivationBlockedTags` 包含 `State.Frozen`；移动和 AI 读取该标签停止。

---

## 伤害后使敌人攻击自己人（魅惑）

- **归类**：同冻结，施加的是 `GE_Charm`。复用 `ApplyEffectOnDamage`，只改配置。
- **依赖**：**阵营系统**（`IsHostile`）。这是唯一需要真正补底层的案例。

```cpp
FGameplayTag GetEffectiveTeam(const AActor* Actor)
{
    if (Actor 有 State.Charmed) return 魅惑者的阵营;
    return 基础阵营;
}

GE_Charm（UCC_BuffDefinition 蓝图）
  授予标签 State.Charmed；施加条件：目标没有 Immunity.Charm
  Behavior：OnApplied 记录魅惑者并清空当前目标（强制重新索敌）；OnRemoved 清空当前目标
```

- 被魅惑者的 AI、攻击、箭都不需要修改，因为它们都通过 `IsHostile` 判断敌我。

---

## 攻击可以反弹弓箭

- **归类**：状态层（开关式能力）。Buff 授予 `Ability.Deflect.Projectile`。
- **依赖**：阵营系统；近战判定能检测投射物；箭支持改变归属。

```cpp
// UCC_Primary 判定时
if (ASC->HasMatchingGameplayTag(Ability.Deflect.Projectile))
    for (Arrow : 判定区域内的投射物)
        if (IsHostile(Owner, Arrow->GetInstigator())) Arrow->Deflect(Owner);

void ACC_ArrowProjectile::Deflect(AActor* NewOwner)
{
    if (bDeflected) return;   // 只能反弹一次
    bDeflected = true;
    OriginalShooter = GetInstigator();
    SetInstigator(NewOwner); SetOwner(NewOwner);
    Payload.SourceTags.AddTag(Damage.Source.Deflected);
    速度改为指向 OriginalShooter；更新碰撞忽略列表;
}
```

- **组合**：反弹的箭造成伤害会触发 `DamageDealt`。冻结 Buff 的 `RequiredSource` 是否包含 Projectile，决定反弹箭能否冻结敌人，只需改配置。

---

## 召唤

### 瞬时召唤（已实现）

- **归类**：能力层。召唤 GE 用 Grant Abilities 授予 `UCC_SummonAlly`，GA 带 `ActivateOnGiven` 授予即激活。
- **流程**：服务器在玩家身边生成召唤物并 `SetTeamOverride(施放者阵营)` → 下一帧移除授予它的 GE（Buff 用完即消失）。
- **Buff 共享**：`UCC_SummonBuffShareComponent` 只用 BuffComponent 公开接口，把玩家的每次获得复制给召唤物，复制品独立计时；带 `Buff.NotShared` 的不同步。
- **生命周期**：召唤物不随 Buff 消失（它是这次施放的结果，不是 Buff 授予的状态）。
- 详见 [SummonAlly.zh-CN.md](SummonAlly.zh-CN.md)。

### 持续召唤（旧功能的迁移目标）

- **归类**：规则层生命周期类，纯服务器逻辑。
- **现状**：`SummonClass` 等字段在 `UCC_BuffDefinition` 中，逻辑在 `UCC_BuffComponent::StartSpecials`。
- **目标**：迁移为 `UCC_BuffBehavior_Summon`，`OnApplied` 生成、`OnRemoved` 按配置销毁；召唤物阵营继承召唤者。
