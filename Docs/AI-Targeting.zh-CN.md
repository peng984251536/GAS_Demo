# AI 寻敌逻辑

## 1. 结构

```mermaid
flowchart TB
    subgraph Triggers["触发（什么时候重新选目标）"]
        direction LR
        T1["感知事件<br/>OnTargetPerceptionUpdated<br/>看见 / 看不见某个 Actor"]
        T2["自己的 ASC 标签事件<br/>CCTags.Status.Charmed 增减<br/>自己被魅惑 / 解除"]
        T3["低频计时器（0.5 秒）<br/>别人被魅惑、死亡等<br/>收不到通知的变化"]
    end

    subgraph Controller["ACC_AIController"]
        direction TB
        RE["ReevaluateTarget<br/>唯一的选目标入口"]
        SEL["SelectBestTarget（静态，可单测）<br/>视野内候选 → 过滤 → 取最近<br/>当前目标有迟滞保护"]
        VAL["IsValidTarget<br/>是 CC_BaseCharacter · 活着 · IsHostile"]
        APPLY["ApplyTarget<br/>唯一写入目标的地方"]
        CUR[("CurrentTarget<br/>弱引用")]
        RE --> SEL --> VAL
        SEL --> APPLY --> CUR
    end

    subgraph Team["UCC_TeamStatics（新）"]
        direction TB
        HOST["IsHostile(A, B)<br/>双方有效阵营都存在且不同"]
        EFF["GetEffectiveTeam<br/>被魅惑 → 施加魅惑者的阵营<br/>否则 → 基础阵营"]
        BASE["GetBaseTeam<br/>CharacterConfig.Team（可选覆盖）<br/>为空 → 角色类默认：玩家 / 敌人"]
        HOST --> EFF --> BASE
    end

    Perc["AIPerceptionComponent<br/>视觉：敌 / 中 / 友 全部检测<br/>GetCurrentlyPerceivedActors"]
    ASC["角色 ASC<br/>Status.Charmed 标签 · 活动 GE 的 Instigator"]

    subgraph Outputs["输出（下游只读这里）"]
        direction LR
        BB["Blackboard<br/>TargetActor · LastKnownLocation"]
        CA["BaseCharacter::SetClosestActor"]
    end
    BT["行为树<br/>巡逻 / 追击 / 搜索"]
    GA["UCC_BowShoot 等技能<br/>GetClosestActor"]
    Old["CC_FindRangedTarget / CC_FindPlayerTarget<br/>旧方案，已不激活，不改"]

    T1 --> RE
    T2 --> RE
    T3 --> RE
    Perc -- "候选列表" --> SEL
    VAL --> HOST
    EFF -- "读标签与 Instigator" --> ASC
    APPLY --> BB --> BT
    APPLY --> CA --> GA

    classDef legacy fill:#eee,stroke:#999,color:#777,stroke-dasharray: 4 3
    class Old legacy
```

## 2. ReevaluateTarget 的判断流程

```mermaid
flowchart TD
    Start(["ReevaluateTarget(丢失位置?)"]) --> BBok{"Blackboard 有效？"}
    BBok -- 否 --> Skip["不改 CurrentTarget<br/>等下一次触发再试"]
    BBok -- 是 --> Get["取视野内候选<br/>GetCurrentlyPerceivedActors"]
    Get --> Loop["逐个 IsValidTarget：<br/>是角色 · 活着 · IsHostile(自己, 候选)"]
    Loop --> Near["候选中最近的 = Best"]
    Near --> Hyst{"当前目标仍在候选中且有效，<br/>并且 距离差 < 迟滞(200)？"}
    Hyst -- 是 --> Keep["Best = 当前目标（不切换）"]
    Hyst -- 否 --> Same{"Best == 当前目标？"}
    Keep --> Same
    Same -- 是 --> Done(["不写任何东西"])
    Same -- 否 --> HasBest{"Best 存在？"}
    HasBest -- 是 --> SetT["TargetActor = Best<br/>LastKnownLocation = Best 位置<br/>SetClosestActor(Best)"]
    HasBest -- 否 --> Lost{"本次是当前目标<br/>离开视野？"}
    Lost -- 是 --> Search["清空 TargetActor<br/>LastKnownLocation = 丢失位置<br/>→ 行为树进入搜索"]
    Lost -- 否 --> Clear["清空 TargetActor 和 LastKnownLocation<br/>（目标死亡 / 变成友方，不去搜索）"]
```

## 3. 时序

### 3.1 看见玩家，随从离开视野（修复"丢任何一个就清空目标"）

```mermaid
sequenceDiagram
    autonumber
    participant P as AIPerception
    participant C as ACC_AIController
    participant T as UCC_TeamStatics
    participant BB as Blackboard / ClosestActor

    P->>C: OnTargetPerceptionUpdated(玩家, 看见)
    C->>C: ReevaluateTarget()
    C->>T: IsHostile(自己, 玩家)
    T-->>C: true（敌人 vs 玩家）
    C->>BB: TargetActor = 玩家，SetClosestActor(玩家)

    P->>C: OnTargetPerceptionUpdated(友方随从, 看不见)
    Note over C: 丢失的不是当前目标 → 不记录丢失位置
    C->>C: ReevaluateTarget()
    C->>C: 最佳仍是玩家 == 当前目标
    Note over BB: 不写任何东西（旧代码这里会清空目标）

    P->>C: OnTargetPerceptionUpdated(玩家, 看不见)
    Note over C: 丢失的正是当前目标 → 记下 Stimulus 位置
    C->>C: ReevaluateTarget(丢失位置)
    C->>BB: 清空 TargetActor，LastKnownLocation = 丢失位置
    Note over BB: 行为树进入"搜索"分支
```

### 3.2 魅惑：自己被魅惑 / 视野里的同伴被魅惑

```mermaid
sequenceDiagram
    autonumber
    participant ASC as 自己的 ASC
    participant C as ACC_AIController
    participant T as UCC_TeamStatics
    participant BB as Blackboard / ClosestActor

    rect rgba(127,127,127,0.08)
    Note over ASC,BB: 自己被玩家魅惑（GE 授予 Status.Charmed，Instigator = 玩家）
    ASC-->>C: Status.Charmed 计数 0 → 1（标签事件，立即）
    C->>C: ReevaluateTarget()
    C->>T: IsHostile(自己, 玩家)
    T->>ASC: 找授予 Charmed 的活动 GE → Instigator = 玩家
    T-->>C: false（自己的有效阵营 = 玩家阵营）
    C->>T: IsHostile(自己, 视野内的敌人同伴)
    T-->>C: true
    C->>BB: TargetActor = 最近的敌人同伴
    end

    rect rgba(127,127,127,0.08)
    Note over ASC,BB: 视野里的同伴 B 被魅惑（自己收不到 B 的标签事件）
    C->>C: 计时器 0.5 秒 → ReevaluateTarget()
    C->>T: IsHostile(自己, B)
    T-->>C: true（B 的有效阵营变成玩家阵营）
    C->>C: B 比当前目标近超过迟滞距离 → 切换
    C->>BB: TargetActor = B
    end
```

### 3.3 目标死亡

```mermaid
sequenceDiagram
    autonumber
    participant C as ACC_AIController
    participant BB as Blackboard / ClosestActor

    Note over C: 玩家死亡，但尸体仍在视野内，不会产生感知事件
    C->>C: 计时器 0.5 秒 → ReevaluateTarget()
    C->>C: IsValidTarget(玩家) = false（IsAlive 为 false）
    alt 视野内还有其他敌对目标
        C->>BB: 切换到最近的那个
    else 没有
        C->>BB: 清空 TargetActor 和 LastKnownLocation（不去尸体处搜索）
    end
```

## 4. 需要你确认的行为变化

1. **敌人之间不再互相攻击。**
   - 现在 `ACC_EnemyCharacter` 构造时被打上 `Follower` 标签，而控制器会把 `Player` 或 `Follower` 都当作目标，所以敌人会锁定视野里的其他敌人。
   - 改完后，敌人默认属于 `CCTags.Team.Enemy`，玩家默认属于 `CCTags.Team.Player`，同阵营不再互打。
   - 如果你的"随从"是站在玩家一边的 AI，在它的 CharacterConfig 里把 `Team` 设成 `CCTags.Team.Player` 即可。
   - 如果同一个敌人蓝图要临时站到玩家一边（召唤物），生成时调用 `ACC_BaseCharacter::SetTeamOverride`，优先级高于 CharacterConfig，见 [SummonAlly.zh-CN.md](SummonAlly.zh-CN.md)。
2. **目标死亡或变成友方时，`LastKnownLocation` 会被清空。** 旧代码不会清空它，所以玩家死后，AI 会走到尸体处去搜索。
3. **魅惑只在服务器生效。** 阵营判断要读活动 GE，而敌人的 ASC 是 Minimal 复制模式，客户端拿不到活动 GE。AI、伤害、索敌本来就只在服务器运行，所以没有影响。
