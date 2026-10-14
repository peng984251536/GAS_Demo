# Buff 系统架构

本文描述 Buff 系统重构后的**目标结构**。原则见 `BuffSystem.zh-CN.md`，案例见 `BuffSystem-Cookbook.zh-CN.md`，当前代码离目标还差什么见 `BuffSystem-Roadmap.zh-CN.md`。

图中标注 `[新]` 的类型尚未实现。

## 1. 架构总览

依赖方向只有一条：**Buff → 标签 / 属性 / 钩子上下文 ← 系统**。粗箭头是钩子调用（系统 → BuffComponent），图中的"玩法系统"不引用任何具体 Buff。

```mermaid
flowchart TB
    subgraph Assets["① 只读配置（编辑器资产）"]
        direction LR
        GE["GE_Buff_*<br/>普通 GE 蓝图<br/>Duration · Stacking · Modifiers · Period"]
        NativeComp["原生 GE 组件<br/>TargetTags · Immunity<br/>Abilities · RemoveOther"]
        UIData["UCC_BuffUIData [新]<br/>名称 · 描述 · 图标"]
        Rules["UCC_BuffRulesComponent [新]<br/>Behaviors 模板 · bRemoveOnDeath"]
        CharCfg["UCC_CharacterConfig<br/>标签 → 动作集 覆盖表"]
        GE --- NativeComp
        GE --- UIData
        GE --- Rules
    end

    Pickup["② 获取：ACC_BuffPickup / 选择系统"]

    subgraph Core["③ Buff 运行时 + GAS（服务器权威）"]
        direction LR
        subgraph BuffRT["每个角色一份"]
            direction TB
            BC["UCC_BuffComponent<br/>Instances · HookIndex<br/>按标签查询 / 驱散"]
            Inst["FCC_BuffInstance<br/>Behavior 实例 + 自动回收的资源"]
            Beh["UCC_BuffBehavior 子类<br/>SplitArrow · ApplyEffectOnDamage · Summon …"]
            Rep["ReplicatedBuffs（FastArray）"]
            BC --> Inst --> Beh
            BC --> Rep
        end
        subgraph GAS["GAS"]
            direction TB
            ASC["ASC<br/>ActiveGE · 标签计数 · 授予的 GA"]
            Exec["UCC_DamageExecution"]
            Attr["UCC_AttributeSet<br/>PostGameplayEffectExecute"]
            Exec --> Attr
        end
    end

    subgraph Systems["④ 玩法系统（不认识任何 Buff，敌我一律用 IsHostile）"]
        direction LR
        Bow["UCC_BowShoot<br/>Build → Modify → Resolve → Execute"]
        Arrow["ACC_ArrowProjectile<br/>携带 Payload 快照"]
        Melee["UCC_Primary 近战"]
        Dmg["UCC_DamageStatics::ApplyDamage<br/>唯一伤害入口"]
        Move["移动 · AI · 索敌"]
        Bow --> Arrow --> Dmg
        Melee --> Dmg
    end

    subgraph Client["⑤ 客户端表现"]
        direction LR
        BuffBar["UCC_BuffBarWidget"]
        Cue["GameplayCue"]
    end

    Pickup -- "ApplyBuff(GE 类, 来源, 等级)" --> BC
    BC -- "ApplySpec" --> ASC
    ASC -- "ActiveGE 增删 · 叠层 · 抑制" --> BC
    Rules -. "模板 DuplicateObject" .-> Inst
    CharCfg -. "标签 → 动作集" .-> Bow

    Bow == "RunHook(Hook.Projectile.Fire)" ==> BC
    Exec == "RunHook(Hook.Damage.*)" ==> BC
    Dmg --> Exec
    Attr -- "Event.Combat.DamageDealt / Taken" --> ASC
    ASC -- "GameplayEvent" --> Beh
    ASC -- "State.* 标签" --> Systems

    Rep -- "复制" --> BuffBar
    ASC -- "复制" --> Cue
```

### 各部分职责

| 部分 | 负责 | 不负责 |
|---|---|---|
| GE 资产 | 数值、标签、免疫、堆叠、持续时间、周期 | 任何运行时状态 |
| `UCC_BuffUIData` | 显示信息 | 玩法 |
| `UCC_BuffRulesComponent` | 声明这个 Buff 挂哪些 Behavior、死亡是否移除 | 保存实例数据（它是共享模板） |
| `UCC_BuffComponent` | 实例表、钩子分发、UI 复制、按标签查询和驱散 | 召唤、动作集、移速等具体玩法 |
| `UCC_BuffBehavior` | 规则层逻辑：改上下文、监听事件、管理自己创建的资源 | 判断别的 Buff 是否存在 |
| 玩法系统 | 在决策点构造上下文、调用 `RunHook`、按结果执行 | 判断具体 Buff |

## 2. 核心类关系

```mermaid
classDiagram
    direction LR

    class UGameplayEffect
    class UGameplayEffectComponent
    class UGameplayEffectUIData

    class UCC_BuffUIData {
        +FText DisplayName
        +FText Description
        +UTexture2D* Icon
        +bool bHiddenInUI
    }
    class UCC_BuffRulesComponent {
        +TArray~UCC_BuffBehavior*~ Behaviors
        +bool bRemoveOnDeath
        +bool bPersistAcrossLevels
    }
    UGameplayEffectComponent <|-- UGameplayEffectUIData
    UGameplayEffectUIData <|-- UCC_BuffUIData
    UGameplayEffectComponent <|-- UCC_BuffRulesComponent
    UGameplayEffect o-- UCC_BuffUIData : GEComponents
    UGameplayEffect o-- UCC_BuffRulesComponent : GEComponents

    class UCC_BuffComponent {
        +ApplyBuff(GEClass, SourceASC, Level, SetByCaller)
        +GetStackCountByTag(Tag) int
        +RemoveBuffsByQuery(Query, MaxCount) int
        +ClearBuffs(Reason)
        +RunHook~TCtx~(HookTag, Ctx)
        -TMap~Handle, FCC_BuffInstance~ Instances
        -TMap~Tag, TArray~FCC_HookEntry~~ HookIndex
        -FCC_ReplicatedBuffArray ReplicatedBuffs
    }
    class FCC_BuffInstance {
        +FActiveGameplayEffectHandle Handle
        +TArray~UCC_BuffBehavior*~ Behaviors
        +FCC_OwnedResources Resources
        +bool bActive
    }
    class FCC_BuffInstanceView {
        +ACC_BaseCharacter* Owner
        +AActor* Instigator
        +FActiveGameplayEffectHandle Handle
        +int32 StackCount
        +float Level
        +GetSetByCaller(Tag) float
    }
    class UCC_BuffBehavior {
        <<abstract>>
        +FGameplayTagContainer HandledHooks
        +int32 HookPriority
        +OnActivated(View)
        +OnDeactivated(View)
        +OnStackChanged(View, Old, New)
        +OnRefreshed(View)
        +ModifyHook(View, HookTag, FInstancedStruct& Ctx)
        #ListenGameplayEvent(Tag, Fn)
        #StartTimer(Interval, bLoop, Fn)
        #SpawnOwnedActor(Class, Transform)
        #ApplyChildEffect(TargetASC, GEClass)
        #ConsumeStacks(Count)
    }
    class UCC_BuffBehavior_SplitArrow {
        +int32 ExtraArrows
        +float SpreadAngle
        +float DamagePerArrowRatio
    }
    class FCC_ProjectileFireContext {
        +Shooter / Target / Origin / AimDirection
        +int32 ProjectileCount
        +float SpreadAngle
        +FCC_ArrowPayload Payload
    }

    UCC_BuffComponent "1" *-- "*" FCC_BuffInstance
    FCC_BuffInstance "1" *-- "*" UCC_BuffBehavior : 每个 ActiveGE 复制一份
    UCC_BuffRulesComponent ..> UCC_BuffBehavior : 只读模板
    UCC_BuffBehavior ..> FCC_BuffInstanceView : 回调参数
    UCC_BuffBehavior <|-- UCC_BuffBehavior_SplitArrow
    UCC_BuffBehavior_SplitArrow ..> FCC_ProjectileFireContext : 修改
```

### 三种 Context 不要混用

| 名字 | 是什么 | 生命周期 |
|---|---|---|
| `FCC_BuffInstanceView` | "这一份 Buff"的只读信息，传给 Behavior | 跟着 ActiveGE |
| `FCC_*HookContext` / `FCC_ProjectileFireContext` | 某个决策点"这一次"的可修改参数 | 一次 `RunHook` 调用 |
| `FCC_GameplayEffectContext` | 随 GE 传递的来源信息：`SourceTags`、`ProcDepth`、`ProcChainMask`、`VolleyId` | 一次伤害 / 一次施加 |

## 3. 时序：弓箭手获得分裂箭之后

示例配置：

- `GE_Buff_SplitArrow`：持续 20 秒，按来源叠加，最多 3 层，重复施加刷新持续时间。
- `UCC_BuffUIData`：名称"分裂箭"。
- `UCC_BuffRulesComponent.Behaviors = [ UCC_BuffBehavior_SplitArrow { ExtraArrows = 2, SpreadAngle = 30, DamagePerArrowRatio = 0.6 } ]`，`HandledHooks = Hook.Projectile.Fire`。

以下除"客户端"一列外都在服务器执行。

### 3.1 获得 Buff

```mermaid
sequenceDiagram
    autonumber
    participant P as ACC_BuffPickup
    participant BC as UCC_BuffComponent
    participant ASC as 弓箭手 ASC
    participant B as SplitArrow 实例
    participant C as 客户端 BuffBar

    P->>BC: ApplyBuff(GE_Buff_SplitArrow, nullptr, Level 1)
    BC->>BC: 校验 权威端 / 存活 / 非清理中
    BC->>ASC: MakeOutgoingSpec + ApplyGameplayEffectSpecToTarget
    Note over ASC: GAS 原生处理<br/>施加条件 · 免疫 · 堆叠 · 持续时间 · 授予标签
    ASC-->>BC: OnActiveGameplayEffectAdded(Handle)
    BC->>BC: GE 上找到 UCC_BuffRulesComponent，新建 FCC_BuffInstance
    BC->>B: DuplicateObject(模板, Outer = BuffComponent)
    BC->>B: OnActivated(View: Owner, Instigator, Stack = 1, Level = 1)
    BC->>BC: HookIndex[Hook.Projectile.Fire] 插入 B（按 Priority、实例号排序）
    BC->>BC: ReplicatedBuffs 新增条目（UIData、层数、到期时间）
    BC-->>C: FastArray 增量复制
    C->>C: PostReplicatedAdd → 显示"分裂箭"图标和倒计时
```

### 3.2 放箭：Build → Modify → Resolve → Execute

`UCC_BowShoot` 全程不知道分裂箭存在，它只知道"放箭这一步有个钩子"。

```mermaid
sequenceDiagram
    autonumber
    participant AI as 弓箭手 AI
    participant GA as UCC_BowShoot
    participant BC as UCC_BuffComponent
    participant B as SplitArrow 实例
    participant A as ACC_ArrowProjectile ×N

    AI->>GA: 激活技能（ActivationBlockedTags 不含当前状态）
    GA->>GA: 播放蒙太奇，WaitGameplayEvent(放箭事件)
    Note over GA: 动画到达放箭帧
    GA->>GA: ① Build：读 CharacterConfig + OwnedAction<br/>Count = 1, Spread = 0, Payload.Damage = OwnedAction.Damage<br/>Payload.SourceTags = Damage.Source.Projectile, VolleyId = 新编号
    GA->>BC: ② RunHook(Hook.Projectile.Fire, Ctx)
    loop HookIndex[Hook.Projectile.Fire] 中每个处于激活状态的 Behavior
        BC->>B: ModifyHook(View{Stack = 1}, Hook, Ctx)
        B->>B: Count += ExtraArrows × Stack（合并：相加）→ 3<br/>Spread = max(Spread, 30)（合并：取最大）<br/>Payload.Damage ×= 0.6（合并：相乘）
    end
    BC-->>GA: Ctx 已被所有 Behavior 修改
    GA->>GA: ③ Resolve：Count = 3、Spread = 30° → 3 个方向 −15° / 0° / +15°
    loop 每个方向
        GA->>A: ④ Execute：SpawnActorDeferred + 写入 Payload 副本 + FinishSpawning
    end
    GA->>GA: 蒙太奇结束 → EndAbility
```

如果同时还有爆炸箭，它也挂在 `Hook.Projectile.Fire` 上，在同一个循环里把 `Payload.ExplosionRadius` 写进去。结果就是"3 支会爆炸的箭"，两个 Behavior 互相不知道对方。

### 3.3 命中：只用 Payload，不回头查射手

```mermaid
sequenceDiagram
    autonumber
    participant A as ACC_ArrowProjectile
    participant T as UCC_TeamStatics
    participant D as UCC_DamageStatics
    participant X as UCC_DamageExecution
    participant SBC as 射手 BuffComponent
    participant TBC as 目标 BuffComponent
    participant AS as 目标 AttributeSet
    participant ASC as 射手 / 目标 ASC

    A->>T: IsHostile(Instigator, OtherActor)
    T-->>A: true
    A->>A: VolleyId 去重：同一齐射对同一目标只结算一次（可选）
    A->>D: ApplyDamage(Request{ FlatDamage = Payload.Damage, SourceTags = Projectile, VolleyId })
    D->>X: 应用伤害 GE（Context 带 SourceTags / ProcDepth = 0）
    X->>SBC: RunHook(Hook.Damage.Outgoing, DmgCtx)
    X->>TBC: RunHook(Hook.Damage.Incoming, DmgCtx)
    X->>X: 最终伤害 = (Base × (1 + Increased) × More + Flat) − 防御
    X->>AS: 输出 Health 修改
    AS->>AS: PostGameplayEffectExecute：扣护盾 / 扣血 / 判死
    AS->>ASC: Event.Combat.DamageDealt → 射手，Event.Combat.DamageTaken → 目标
    Note over ASC: 监听这些事件的 Behavior（比如命中冻结）在这里触发<br/>它们施加的伤害 ProcDepth + 1，防止循环
    A->>A: Destroy()
```

箭飞行途中分裂箭到期、甚至射手死亡，都不影响已发射的箭，因为它命中时只读自己的 Payload。

### 3.4 叠层、抑制与结束

```mermaid
sequenceDiagram
    autonumber
    participant P as ACC_BuffPickup
    participant BC as UCC_BuffComponent
    participant ASC as 弓箭手 ASC
    participant B as SplitArrow 实例
    participant C as 客户端 BuffBar

    rect rgba(127,127,127,0.08)
    Note over P,C: 再次拾取：叠层
    P->>BC: ApplyBuff(GE_Buff_SplitArrow)
    BC->>ASC: ApplyGameplayEffectSpecToTarget
    ASC-->>BC: OnStackChanged(Handle, 1 → 2)（同一个 Handle，不新建实例）
    BC->>B: OnStackChanged(View, 1, 2)
    BC-->>C: 条目层数 = 2，到期时间刷新
    Note over B: 下一次放箭 ModifyHook 收到 Stack = 2<br/>Count = 1 + 2 × 2 = 5
    end

    rect rgba(127,127,127,0.08)
    Note over P,C: 被抑制（例如进入"禁用增益"区域）
    ASC-->>BC: OnInhibitionChanged(Handle, true)
    BC->>B: OnDeactivated(View)
    BC->>BC: 回收 B 自动登记的资源，HookIndex 移除 B
    Note over BC: 放箭恢复为 1 支
    ASC-->>BC: OnInhibitionChanged(Handle, false)
    BC->>B: OnActivated(View)，HookIndex 重新插入
    end

    rect rgba(127,127,127,0.08)
    Note over P,C: 到期 / 驱散 / 死亡 / EndPlay —— 走同一条路径
    ASC-->>BC: OnAnyGameplayEffectRemoved(Handle)
    BC->>B: OnDeactivated(View)
    BC->>BC: 回收资源（订阅 · 计时器 · 生成物 · 子 GE）
    BC->>BC: HookIndex 移除 B，Instances 删除条目，B 交给 GC
    BC-->>C: FastArray 删除条目
    C->>C: PostReplicatedRemove → 移除图标
    end
```

### 生命周期对照（设计原则第 9 节）

| 路径 | 触发 | BuffComponent 做什么 |
|---|---|---|
| 正常到期 | `OnAnyGameplayEffectRemoved` | `OnDeactivated` → 回收 → 删实例 |
| 驱散 / 主动移除 | `RemoveBuffsByQuery` → GAS 移除 → 同上 | 同上 |
| 抑制 / 恢复 | `OnInhibitionChanged` | `OnDeactivated` / `OnActivated`，保留实例 |
| 叠层增减 | `OnStackChanged` | `OnStackChanged`，不重建实例 |
| 持有者死亡 | `ClearBuffs(Death)`，只移除 `bRemoveOnDeath` 的 | 同"到期" |
| 关卡切换 / EndPlay | `ClearBuffs(EndPlay)` | 同"到期" |
| 放箭过程中被移除 | — | 下一次 `RunHook` 生效；已发射的箭不受影响（快照） |
