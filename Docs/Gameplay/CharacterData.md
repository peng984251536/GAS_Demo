# 角色默认配置与运行时数据

本次只修改代码，没有运行编译、自动化测试或编辑器验证。

## 命名与职责

| 类型 | 父类 | 保存内容 |
| --- | --- | --- |
| `UCC_CharacterConfig`（原 `UCC_PawnData`） | `UPrimaryDataAsset` | 初始技能列表、属性初始化 GameplayEffect；多个角色可共享 |
| `UCC_CharacterRuntimeData` | `UObject` | 存活、受击、最近移动方向、当前目标查询结果；每个角色独立 |
| `FCC_CharacterAbilityEntry`（原 `FCC_PawnAbilityEntry`） | `USTRUCT` | 技能类、初始等级 |
| `FClosestActorWithTagResult` | `USTRUCT` | 目标弱引用、查询时的距离快照 |

“默认配置”仍然可以在游戏运行时读取；区别在于它不保存随角色变化的实例状态。

```text
ACC_BaseCharacter
├─ CharacterConfig → 共享的 UCC_CharacterConfig 数据资产
└─ RuntimeData → 当前角色自己的 UCC_CharacterRuntimeData 默认子对象
```

运行时对象在角色构造函数中创建：

```cpp
RuntimeData = CreateDefaultSubobject<UCC_CharacterRuntimeData>(TEXT("CharacterRuntimeData"));
```

## 使用入口

- `GetCharacterConfig()`：取得只读配置；C++ 子类可以覆盖配置来源。
- `GetRuntimeData()`：取得当前角色的运行时对象。蓝图只读取状态，修改使用角色的行为入口。
- `IsAlive()`、`IsHit()`、`GetLastMoveInputDirection()`：保留在角色上，转发到 RuntimeData。
- `HandleDeath()`、`HandleHit()`、`HandleRefresh()`、`HandleRespawn()`：由服务器调用，更新当前角色状态；没有新增客户端到服务器的 RPC。
- `SetClosestActor()`、`GetClosestActor()`：统一放在基类，敌人继承使用。查询返回副本，避免绕过 Setter 修改缓存。
- `SetLastMoveInputDirection()`：仍允许本地输入或服务器 AI 更新自己的缓存。

死亡时清除受击状态和目标；重生时重置存活、受击、移动方向和目标缓存，然后应用初始属性 GE。
保留原有技能授予和属性初始化时机，不增加重复授予/撤销技能的机制，也不恢复你已经注释掉的“重生清空所有 GE”逻辑。

## 蓝图与资产迁移

`Config/DefaultEngine.ini` 添加了类、结构体、角色配置属性和相关函数的 Core Redirects。
现有 `DA_PawnData_Player`、`DA_PawnData_Enemy` 的包路径保持不变，加载时将旧类解析为 `CC_CharacterConfig`。
旧角色属性 `PawnData` 对应新属性 `CharacterConfig`，旧 `GetPawnData` 对应 `GetCharacterConfig`。
本次没有运行编辑器，也没有重新保存或改名 `.uasset`。

你编译并打开编辑器后，新配置位置是角色类默认值的 `Crash | Data → Character Config`。
以后创建配置资产时选择 `CC_CharacterConfig`，可命名为 `DA_CharacterConfig_Player` 等。

初始属性效果属于配置数据。旧角色蓝图可能仍把 GE 保存在角色属性中，因此保留原名
`InitializeAttributesEffect` 作为兼容入口，显示在 `Crash | Legacy`。
初始化优先读取 CharacterConfig 的 GE，未指定时回退到旧字段；把原 GE 填入配置资产后，新角色只需要配置 CharacterConfig。
跨对象移动属性不能仅靠改名重定向搬迁数据，因此本次不会声称已经把旧蓝图的 GE 写入数据资产。

如果你已经在蓝图里直接读取了旧 Character / PawnData 上的 `bAlive`、`bHit`、`LastMoveInputDirection` 或敌人的 `FClosestActor` 属性，需要把这些节点改成角色查询函数，或通过 `GetRuntimeData()` 读取新对象。
旧 PawnData 上的状态函数也应改为 RuntimeData 上的函数；它们不再属于配置资产。
旧目标查询节点若保留了执行引脚，需要刷新或重新放置为新的纯查询节点。

## 运行时状态与同步

RuntimeData 使用 `Transient` 字段保存状态，并通过所属角色的注册子对象列表复制。
角色在 `PostInitializeComponents()` 注册，在 `EndPlay()` 注销；配置资产不参与状态复制。

- `bAlive`、`bHit`：服务器向客户端复制。
- `LastMoveInputDirection`：服务器向非拥有客户端复制，避免覆盖拥有客户端的本地输入缓存。此设置不会把客户端输入自动发送到服务器。
- `ClosestActor`：当前仅是服务器 AI 缓存，不复制。需要客户端锁定 UI 时，再明确同步目标的需求。
- 为启用 Iris 的构建提供了复制片段注册，并在模块中加入 `SetupIrisSupport(Target)`。

当前数据对象不负责维护 GAS 状态 Tag。已有能力/效果对 Tag 的管理继续沿用；以后若合并状态来源，应统一修改入口。

`FClosestActorWithTagResult` 已移到独立的 `Data/CC_TargetingTypes.h`，反射名称和字段名保持不变。
空结果距离统一为最大浮点值；使用前检查 Actor 有效性，攻击判断需要重新计算实时距离并检查目标是否存活。

## 后续扩展

- 可供多角色共享的初始参数或资产引用：添加到 CharacterConfig。
- 当前角色会变化的数据：添加到 RuntimeData，并明确默认值、重置方式以及是否需要复制。
- 需要属性监听、事件、复杂生命周期的独立功能：再按职责提取组件。
