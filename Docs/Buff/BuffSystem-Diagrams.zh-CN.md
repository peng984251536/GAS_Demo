# Buff 系统结构图、序列图与类图

依据当前项目代码绘制；范围仅为已经实现的 Buff 核心。

配置使用普通 GameplayEffect，原生 GAS 负责属性、标签、持续时间、堆叠与周期。BuffComponent 观察活动 GE，管理独立 Behavior、通用钩子及 UI 快照。没有画入分裂箭、伤害管线、阵营、召唤等具体玩法扩展。

## 1. 结构图

蓝色为只读配置，绿色为服务器运行时，黄色为客户端显示。

```mermaid
flowchart TB
    subgraph Config["只读配置：普通 GameplayEffect 资产"]
        GE["UGameplayEffect<br/>数值 · 标签 · 持续时间 · 叠层 · 周期"]
        Rules["UCC_BuffRulesComponent<br/>Behavior 模板 · bRemoveOnDeath"]
        UIData["UCC_BuffUIData<br/>名称 · 描述 · 图标 · 隐藏"]
        Template["UCC_BuffBehavior 模板"]
        GE --- Rules
        GE --- UIData
        Rules --> Template
    end

    Pickup["ACC_BuffPickup<br/>拾取入口"]
    Character["ACC_BaseCharacter<br/>ASC 初始化 · 死亡 · 重生"]

    subgraph Server["服务器：实际 Buff 状态"]
        Source["来源 ASC<br/>创建 Spec"]
        ASC["目标 ASC / GAS<br/>持有 ActiveGE"]
        BC["UCC_BuffComponent<br/>施加 · 查询 · 驱散 · 生命周期"]
        Instances["Instances<br/>GE Handle → FCC_BuffInstance"]
        View["FCC_BuffInstanceView<br/>来源 · 目标 · 层数 · 等级 · 参数"]
        Behavior["独立 Behavior 实例<br/>生命周期回调 · 规则 · 资源登记"]
        Hooks["HookIndex<br/>HookTag → 有序 Behavior 引用"]
        Rep["FCC_ReplicatedBuffArray<br/>GE 类 · 编号 · 标签 · 层数 · 到期时间"]
        BC -->|"创建 Spec"| Source
        Source -->|"ApplyGameplayEffectSpecToTarget"| ASC
        ASC -->|"增删 · 叠层 · 时间 · 抑制事件"| BC
        BC --> Instances
        Instances --> View
        Instances --> Behavior
        BC --> Hooks
        Hooks -.-> Behavior
        BC -->|"更新并标记 Dirty"| Rep
        Behavior -->|"登记与回收"| Resources["订阅 · 计时器 · 持有 Actor · 子 GE"]
    end

    Pickup -->|"ApplyBuff"| BC
    Character -->|"InitializeASC / ClearBuffs"| BC
    GE -.->|"只读配置"| Source
    Template -.->|"每个活动 GE 复制一份"| Behavior
    Caller["通用钩子调用方<br/>传入 HookTag 与本次 Context"]
    Caller -->|"RunHook"| BC
    BC -->|"按序修改 Context"| Behavior
    Behavior -.->|"修改后的 Context"| Caller

    subgraph Client["客户端：Buff 展示"]
        ClientBC["客户端 BuffComponent<br/>展示快照与 OnBuffsChanged"]
        Widget["UCC_BuffBarWidget<br/>图标 · 层数 · 倒计时 · 抑制显示"]
        ClientBC -->|"变化通知 / 查询"| Widget
    end
    Rep ==>|"FastArray 增量复制"| ClientBC
    UIData -.->|"通过 GE 类读取本地配置"| Widget
    ASC -->|"MoveSpeedMultiplier 属性变化"| Adapter["现有移速适配<br/>BuffComponent.UpdateMovementSpeed"]
    Adapter --> Movement["CharacterMovement.MaxWalkSpeed"]

    classDef asset fill:#edf2ff,stroke:#6478b8,color:#152747
    classDef runtime fill:#edf9f1,stroke:#4e9470,color:#183d28
    classDef client fill:#fff5e4,stroke:#b98b3a,color:#513b13
    class GE,Rules,UIData,Template asset
    class BC,Instances,View,Behavior,Hooks,Rep,Source,ASC runtime
    class ClientBC,Widget client
```

注意：资源登记与回收实际位于 Behavior 基类中；没有另设 OwnedResources 管理类。移速倍率适配目前仍保留在 BuffComponent 内。通用 RunHook 已实现，当前没有接入具体战斗系统。

## 2. 序列图

图中“叠层、抑制、移除”是活动 GE 的后续路径，适用于持续或永久效果。

```mermaid
sequenceDiagram
    autonumber
    participant P as 拾取 / 施加入口
    participant BC as BuffComponent（服务器）
    participant S as 来源 ASC
    participant A as 目标 ASC / GAS
    participant B as Behavior 实例
    participant C as 客户端 BuffComponent / UI

    Note over BC,A: ASC 已完成初始化与事件绑定；来源为空时使用目标 ASC
    P->>BC: ApplyBuff(GEClass, SourceASC, Level)
    BC->>BC: 校验权威、存活、配置与参数
    BC->>S: MakeOutgoingSpec + SourceObject / SetByCaller
    S->>A: ApplyGameplayEffectSpecToTarget

    alt GAS 拒绝施加
        A-->>S: 应用失败
        S-->>BC: 失败结果
        BC-->>P: WasSuccessfullyApplied = false
    else Instant 效果
        A->>A: 执行原生 GE
        Note over BC,C: 不创建活动 Buff 实例或 UI 条目；Instant 禁止配置 Behavior
        A-->>S: 应用结果
        S-->>BC: 应用结果
        BC-->>P: 用 WasSuccessfullyApplied 判断成功
    else 持续 / 永久效果：首次新增
        A->>A: 创建 ActiveGE，计算属性、标签及抑制状态
        A-->>BC: HandleEffectAdded(Handle)
        BC->>BC: 创建 Instance；复制各 Behavior 模板；绑定 GE 事件
        BC->>BC: RefreshFromGAS：读取真实层数、等级、时间、抑制
        loop 每个 Behavior
            BC->>B: Initialize(Component, View)
            B->>B: Disabled；OnInitialized()
            opt 当前未被抑制
                BC->>B: Enable()
                B->>B: Enabled；OnEnabled()
            end
        end
        BC->>BC: SetHooks；更新 UI 快照；MarkItemDirty
        BC-->>C: FastArray 增量复制
        C->>C: PostReplicatedReceive → OnBuffsChanged → 刷新展示
        A-->>S: 应用结果
        S-->>BC: 应用结果
        BC-->>P: 返回 GE Handle / 应用结果
    end

    opt 后续叠层 / 时间刷新
        A-->>BC: OnStackChanged / OnTimeChanged
        BC->>BC: RefreshFromGAS
        BC->>B: UpdateView(View, bRefreshed)
        B->>B: 层数变化调用 OnStackChanged；时间变化调用 OnRefreshed
        Note over BC,B: 保留原 Behavior 对象，不再次 Initialize
        BC-->>C: 更新层数与到期时间
    end

    opt 后续抑制与恢复
        A-->>BC: OnInhibitionChanged(true)
        BC->>BC: 从 HookIndex 移除该实例
        BC->>B: Disable(Inhibited)
        B->>B: OnDisabled；ReleaseResources(false)
        Note over B: 清理订阅、计时器、WhileEnabled 资源；保留 Instance 资源
        BC-->>C: bIsInhibited = true

        A-->>BC: OnInhibitionChanged(false)
        BC->>B: UpdateView；Enable()
        B->>B: OnEnabled；重新登记启用期间资源
        BC->>BC: 重新加入 HookIndex
        BC-->>C: bIsInhibited = false
    end

    opt 到期 / 驱散 / 最终清理
        Note over BC,A: 主动移除先调用 RemoveActiveGameplayEffect；到期由 GAS 发起
        A->>A: 移除 ActiveGE，收回原生属性、标签及授予能力
        A-->>BC: HandleEffectRemoved(Effect)
        BC->>BC: FinishInstance：先摘除实例、钩子与 UI 条目
        BC->>B: Shutdown(Reason)
        B->>B: Finished；若仍 Enabled 则 OnDisabled(Ending)
        B->>B: OnShutdown；ReleaseResources(true)
        Note over B: 回收全部持有资源；Independent 子 GE / Actor 不由父 Buff 回收
        BC->>BC: MarkArrayDirty；通知变化
        BC-->>C: 删除展示条目
    end
```

死亡清理有两条分支：
- bRemoveOnDeath=true：移除 GE，走最终清理。
- bRemoveOnDeath=false：保留 GE 和实例，设置 bSuspendedForDeath，移除钩子并 Disable(OwnerDeath)。重生时清理旧 Buff。

EndPlay 会清理并解绑 ASC，组件结束时不再主动发出展示变化通知。WhileEnabled 资源在抑制时回收；Instance 资源在最终结束时回收；Independent 资源自行结束。拥有型子 GE 要求 Stacking=None。

## 3. 类图

同名 Behavior 类型有两种对象：GE 规则组件里的只读模板，以及 BuffComponent 下的运行实例。两者不能共用实例状态。

```mermaid
classDiagram
    direction TB
    class UGameplayEffect {
        +DurationPolicy
        +Modifiers
        +StackingType
        +Period
        +GEComponents
    }
    class UGameplayEffectComponent
    class UGameplayEffectUIData
    class UCC_BuffUIData {
        +DisplayName
        +Description
        +Icon
        +bHiddenInUI
    }
    class UCC_BuffRulesComponent {
        +Behaviors
        +bRemoveOnDeath
        +IsDataValid()
    }
    UGameplayEffectComponent <|-- UGameplayEffectUIData
    UGameplayEffectUIData <|-- UCC_BuffUIData
    UGameplayEffectComponent <|-- UCC_BuffRulesComponent
    UGameplayEffect "1" *-- "0..1" UCC_BuffUIData : 显示组件
    UGameplayEffect "1" *-- "0..1" UCC_BuffRulesComponent : 规则组件

    class UCC_BuffComponent {
        -Instances
        -HookIndex
        -ReplicatedBuffs
        -BoundASC
        +InitializeASC()
        +ApplyBuff()
        +ApplyBuffWithParameters()
        +RemoveBuff()
        +RemoveBuffsByQuery()
        +ClearBuffs()
        +RunHook()
        +GetStackCountByTag()
        +GetActiveBuffs()
        +GetRemainingTime()
        -RefreshFromGAS()
        -FinishInstance()
    }
    class FCC_BuffInstance {
        +View
        +Behaviors
        +bRemoveOnDeath
        +bSuspendedForDeath
    }
    class FCC_BuffInstanceView {
        +Owner
        +Instigator
        +TargetASC
        +SourceASC
        +Handle
        +InstanceId
        +StackCount
        +Level
        +SetByCaller
    }
    class UCC_BuffBehavior {
        <<abstract>>
        +HandledHooks
        +HookPriority
        -View
        -State
        -Component
        -Subscriptions
        -Timers
        -Actors
        -Effects
        -Initialize()
        -Enable()
        -Disable()
        -Shutdown()
        #OnInitialized()
        #OnEnabled()
        #OnDisabled()
        #OnStackChanged()
        #OnRefreshed()
        #OnShutdown()
        #ModifyHook()
        #OnGameplayEvent()
        #ListenGameplayEvent()
        #StartTimer()
        #SpawnOwnedActor()
        #ApplyChildEffect()
        #ConsumeStacks()
    }
    class FHookEntry {
        +Behavior
        +Handle
        +Priority
        +InstanceId
        +BehaviorIndex
    }
    class FInstancedStruct
    class UAbilitySystemComponent {
        +ActiveGameplayEffects
        +MakeOutgoingSpec()
        +ApplyGameplayEffectSpecToTarget()
        +RemoveActiveGameplayEffect()
    }
    class FActiveGameplayEffect
    class FCC_ReplicatedBuffArray {
        +Items
        +Owner
        +NetDeltaSerialize()
        +PostReplicatedReceive()
    }
    class FCC_ReplicatedBuffItem {
        +InstanceId
        +EffectClass
        +BuffTags
        +StackCount
        +Level
        +bIsInhibited
        +EndServerTime
        +Duration
    }
    class UCC_BuffBarWidget {
        -Observed
        +SetObservedCharacter()
        -RefreshBuffs()
        #NativeTick()
    }

    UCC_BuffRulesComponent "1" *-- "*" UCC_BuffBehavior : 只读模板
    UCC_BuffComponent "1" *-- "*" FCC_BuffInstance : GE Handle 索引
    FCC_BuffInstance "1" *-- "*" UCC_BuffBehavior : 运行实例
    FCC_BuffInstance *-- FCC_BuffInstanceView : 状态快照
    UCC_BuffBehavior *-- FCC_BuffInstanceView : 回调前更新的副本
    UCC_BuffComponent *-- FHookEntry : HookIndex
    FHookEntry ..> UCC_BuffBehavior : 弱引用
    UCC_BuffComponent ..> UAbilitySystemComponent : 观察与施加
    UAbilitySystemComponent *-- FActiveGameplayEffect : GAS 实际状态
    FCC_BuffInstanceView ..> FActiveGameplayEffect : Handle 标识
    UCC_BuffComponent *-- FCC_ReplicatedBuffArray
    FCC_ReplicatedBuffArray "1" *-- "*" FCC_ReplicatedBuffItem
    FCC_ReplicatedBuffItem ..> UGameplayEffect : 保存 GE 类
    UCC_BuffComponent ..> FInstancedStruct : 通用钩子参数
    UCC_BuffBehavior ..> FInstancedStruct : 按引用修改
    UCC_BuffBarWidget ..> UCC_BuffComponent : 读取复制快照
    UCC_BuffBarWidget ..> UCC_BuffUIData : 读取 GE 显示组件
```

类图展示主要字段与方法，省略引擎通用成员、委托类型及部分内部辅助函数。HookIndex 按 HookPriority、InstanceId、BehaviorIndex 排序。实际复制的只有 FCC_ReplicatedBuffArray，Behavior、实例表与 GAS Handle 不复制给 UI。

