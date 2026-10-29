# 近战命中事件

近战的检测形状和出伤帧统一放在 `UAnimNotify_MeleeHitBox`。通知在服务器按 Mesh/插槽的世界变换检测 `Shapes`，将各目标的 `FHitResult` 放入 `FGameplayEventData.TargetData`，并向攻击者自身发送 `CCTags.Event.Attack`。

攻击能力蓝图使用以下连接：

```text
WaitGameplayEvent(CCTags.Event.Attack)
  EventReceived → HitBoxOverlapFromEvent → 原有受击与伤害处理
  Payload ─────→ HitBoxOverlapFromEvent.Payload
```

`HitBoxOverlapFromEvent` 只解析通知的结果，不重新查询碰撞。空挥也会发送空 `TargetData`，不会沿用上一次的目标，也不会回退到能力中的球形检测。多个形状、同一角色的多个碰撞组件在同一个通知内按 Actor 去重。不同通知代表不同出伤帧，可以分别命中。

碰撞按 Pawn 对象类型查询，忽略攻击者、友方和死亡角色；阵营继续通过 `UCC_TeamStatics::IsHostile` 判定。表面射线只用于补充受击点/法线，不作为第二次命中资格检测。箭矢反弹仍使用同一套 Shapes，走独立的投射物交互系统，不进入角色伤害列表。

通知对象属于动画资源，不保存角色运行状态。每次事件拥有独立的命中快照；移动目标、下一次触发通知都不会改写已发送的数据。

GameplayEvent 本身不自动复制。服务器负责检测和伤害；客户端仍发送同标签的本地时序事件，但目标数据为空。不要将客户端检测结果直接视为服务器授权的命中。

## 手动接线

- 已有 Melee HitBox 通知的形状、位置和触发时间保持不变，可在蒙太奇预览中直接调整。
- 在实际出伤帧用 Melee HitBox 替换旧的 Attack 事件通知，其他移速/连招/放箭事件保留。不要在同一出伤帧同时保留两个 Attack 通知。
- 将攻击能力中的 `HitBoxOverlapText` 替换为 `HitBoxOverlapFromEvent`，把 WaitGameplayEvent 的 Payload 接到新函数的 Payload 输入；返回数组继续接原来的伤害与受击逻辑，执行引脚保持原来的顺序。
- 旧球形判定半径默认 100、角色前方偏移默认 200；若需要近似原范围，默认 Mesh 朝向下可用球半径 100、Mesh 局部偏移 `(0,200,90)` 作为调整起点。最终以实际角色 Mesh/插槽变换和蒙太奇预览为准。
- 旧 `HitBoxOverlapText`、球形检测参数与旧调试绘制已删除；判定范围和运行时调试统一由 Melee HitBox 通知配置。
- 本次只保留 C++ 改动；此前自动修改的蓝图和蒙太奇已从 `Saved/MeleeEventMigration/Backup/` 恢复。资产接线由使用者手动完成。
- 通知的 `Draw Debug` 可以显示运行时实际检测形状与命中点。

验证：`GAS_Demo.Combat.MeleeHitEvent` 覆盖事件数据、敌我/死亡过滤、多形状去重、空挥、结果快照和球/盒/胶囊形状；`GAS_Demo.Buff.NewOffers.Deflect` 验证箭矢反弹入口未被破坏。
