# 旧版 CC_PawnData 说明（已被替代）

当前实现已经拆分为 `UCC_CharacterConfig` 和 `UCC_CharacterRuntimeData`，请阅读 [角色数据说明](CharacterData.md)。
下文仅保留首次迁移的历史记录；其中的编译/验证结果不适用于本次拆分，本次没有运行验证。

这次只抽出 GameplayAbility 配置。属性初始化效果、动作数据、存活/受击状态仍使用现有实现。

## 已配置的参考资产

| 角色蓝图 | PawnData 资产（位于 `/Game/GAS_Demo/Data/Pawn`） | 技能数 |
| --- | --- | --- |
| `BP_CC_Char01` | `DA_PawnData_Player` | 3 |
| `BP_CC_Enemy02` | `DA_PawnData_Enemy` | 4 |

两份资产按原蓝图列表的顺序填入，初始等级均为 1，角色蓝图的 PawnData 引用已赋值。
可以直接打开这些资产作为参考。旧技能数组保留原值作为兼容备份，指定 PawnData 后不参与授予。
修改前的两个蓝图原文件保存在项目的 `Saved/PawnData/BackupBeforeMigration/Content/GAS_Demo/Blueprints`。

## 数据与执行分工

```text
角色蓝图的 PawnData
    → UCC_PawnData（共享配置资产）
        → StartupAbilities: FCC_PawnAbilityEntry[]
            → Ability + AbilityLevel

ACC_BaseCharacter::GiveStartupAbilities()
    → GetPawnData()
    → ASC::GiveAbility(FGameplayAbilitySpec(Ability, AbilityLevel))
```

- `UCC_PawnData` 继承 `UPrimaryDataAsset`，仅保存默认配置，不保存技能句柄或其他运行时状态。
- `FCC_PawnAbilityEntry` 表示一项技能配置，初始等级默认为 1。
- `GetPawnData()` 是蓝图可读的 C++ 虚函数，返回 const 指针。后续可在子类中覆盖配置来源。
- `GiveStartupAbilities()` 保留现有虚函数入口，只在服务器执行，继续经过现有 ASC 的 `ActivateOnGiven` 自动激活流程。
- 玩家仍在 `PossessedBy()` 初始化 ASC 后授予；敌人仍在 `BeginPlay()` 初始化 ASC 后授予。客户端接收 ASC 的技能复制。

## 在编辑器中配置

1. 完整编译并重新打开编辑器，确保新 UCLASS / USTRUCT 已加载。
2. 内容浏览器 → 杂项（Miscellaneous）→ 数据资产（Data Asset），选择 `CC_PawnData`。
3. 例如创建 `DA_PawnData_Player`、`DA_PawnData_Enemy`。
4. 编辑资产的 `Pawn | Abilities → Startup Abilities`，逐项填写 `Ability` 和 `Ability Level`。
5. 在角色蓝图的类默认值中，将 `Crash | Data → Pawn Data` 指向相应资产。
6. 右键资产执行 Validate Assets，检查空技能类、抽象技能类和小于 1 的等级。

空数组合法，表示没有初始技能。运行时会跳过并记录无效条目，避免向 ASC 授予无效技能。

## 已有蓝图的兼容

原 `StartupAbilities` 属性暂时保留原序列化名称和类型，显示在 `Crash | Abilities | Legacy`。
没有配置 PawnData 的旧蓝图继续按原数组授予等级 1 的技能，并输出迁移提示。

一旦指定 PawnData，完全使用数据资产中的列表，不合并旧数组；新列表为空时也不回退。
手动迁移时，按原顺序复制技能到新资产，再赋值 PawnData。确认所有角色蓝图迁移完成后，可删除 Character 的旧属性和兼容分支。

## 后续扩展位置

- 每项技能各自不同的数据：扩展 `FCC_PawnAbilityEntry`，并在构造 `FGameplayAbilitySpec` 时消费它。
- 角色整体配置：在 `UCC_PawnData` 中增加动作集、输入配置等资产引用。
- 多角色组合复用技能包：以后可引入独立 `UCC_AbilitySet`，由 PawnData 引用多个技能包；目前保留一层列表便于理解。
- 运行时授予结果及撤销：句柄保存在角色/组件/ASC 实例中，不保存在共享数据资产中。

当前 PawnData 通过角色类默认值的硬引用加载，不需要额外配置 Asset Manager 扫描。
如果以后需要通过 Primary Asset ID 动态加载，再配置 Asset Manager 的扫描目录和加载策略。

当前改动只替换配置来源，不实现运行时换 PawnData、重复授予去重或技能撤销。
`GiveStartupAbilities()` 仍应仅在现有初始化入口调用；若后续支持更换 Pawn / 角色职业，需要按授予句柄管理旧技能及自动激活能力的生命周期。

## 本次验证

- UE 5.6 `GAS_DemoEditor Win64 Development` 完整编译通过。
- 两份 PawnData 通过 UE 数据有效性检查。
- 独立 UE 进程重新加载两个角色蓝图，通过 `GetPawnData()` 核对了资产引用、技能顺序和初始等级。
- 验证结果：`Saved/PawnData/verification.json`；构建与验证日志：`Saved/Logs/PawnDataBuild.log`、`Saved/Logs/PawnDataVerification.log`。
- 尚未进行 PIE 联机实测。

参考：[Epic Lyra 的能力系统](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine)。
