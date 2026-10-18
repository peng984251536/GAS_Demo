#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Engagement/CC_EngagementGroup.h"
#include "AI/Engagement/CC_EngagementTypes.h"
#include "CC_EngagementSubsystem.generated.h"

/** 分配变化通知：(攻击者, 新分配)。控制器只处理自己 Pawn 的那一条。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCC_OnEngagementAssignmentChanged, AActor* /*Attacker*/,
	const FCC_EngagementAssignment& /*Assignment*/);

/**
 * 围攻协调者：决定每个攻击者「站哪」和「能不能打」。槽位与令牌的唯一真实来源。
 *
 * - 每个被围攻的目标懒创建一个围攻组（内圈 / 外圈 / 远程圈 + 令牌池），没有攻击者时自动销毁；
 *   玩家、随从、被魅惑的敌人都可以是目标。
 * - 每个组每 UpdateInterval 秒更新一次：清理失效攻击者 → 重建并校验槽位 → 进展检查 →
 *   增量分配槽位 → 最多发放一个令牌 → 通知分配变化。
 * - 令牌由子系统主动发放（发放即占内圈槽位），由控制器在 CCAbilities.Attack 标签 1→0 时带编号归还。
 *
 * 只在服务器上有攻击者注册（AIController 只存在于服务器），客户端上的实例没有任何组，Tick 立即返回。
 * 参数在 DefaultGame.ini 的 [/Script/GAS_Demo.CC_EngagementSubsystem] 段用 Settings=(...) 覆盖。
 * 设计与时序见 Docs/AI-Engagement.zh-CN.md，接入步骤见 Docs/AI-Engagement-Usage.zh-CN.md。
 */
UCLASS(Config = Game)
class GAS_DEMO_API UCC_EngagementSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * 把攻击者加入目标的围攻组。已在同一目标的组中时不做任何事；在别的组中时先移出。
	 * @param Attacker 攻击者（敌人角色）。
	 * @param Target 被围攻的目标。
	 * @param bRanged 是否为弓兵；弓兵站远程圈，不使用令牌。
	 */
	void RegisterAttacker(AActor* Attacker, AActor* Target, bool bRanged);

	/** 把攻击者移出围攻组：归还令牌、释放槽位，并通知档位 None。 */
	void UnregisterAttacker(AActor* Attacker);

	/**
	 * 带编号归还令牌。只有攻击者确实持有令牌、且编号与当前编号一致时才生效，否则忽略。
	 * @param Attacker 攻击者。
	 * @param Generation 开始攻击（Attack 标签 0→1）时记录的分配编号。
	 */
	void ReleaseToken(AActor* Attacker, int32 Generation);

	/**
	 * @return 攻击者当前的分配；未注册时档位为 None。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Engagement")
	FCC_EngagementAssignment GetAssignment(const AActor* Attacker) const;

	const FCC_EngagementSettings& GetSettings() const { return Settings; }

	/** 分配变化时广播（已在内部批量延迟到修改完成后发出）。 */
	FCC_OnEngagementAssignmentChanged OnAssignmentChanged;

private:
	UPROPERTY(Config, EditAnywhere, Category = "AI|Engagement")
	FCC_EngagementSettings Settings;

	/** 目标 → 围攻组。 */
	TMap<FObjectKey, FCC_EngagementGroup> Groups;
	/** 攻击者 → 所在组的目标。 */
	TMap<FObjectKey, FObjectKey> AttackerToTarget;
	/** 待发出的通知；在所有修改完成后统一广播，避免回调中重入修改容器。 */
	TArray<TPair<TWeakObjectPtr<AActor>, FCC_EngagementAssignment>> PendingNotifies;
	int32 NextGeneration = 1;

	float GetNow() const;
	int32 AllocateGeneration();

	FCC_EngagementGroup* FindGroupOf(const FObjectKey& AttackerKey);
	const FCC_EngagementGroup* FindGroupOf(const FObjectKey& AttackerKey) const;

	/** 从组中移除攻击者（归还令牌、释放槽位）。bNotify 为 true 时通知档位 None。 */
	void RemoveAttacker(const FObjectKey& AttackerKey, bool bNotify);

	void UpdateGroup(FCC_EngagementGroup& Group, float GroupDeltaTime);
	void PruneInvalid(FCC_EngagementGroup& Group);
	void RebuildRings(FCC_EngagementGroup& Group, float Now);
	void ValidateRing(FCC_SlotRing& Ring, const FVector& NavCenter, bool bHasNavCenter) const;
	void CheckProgress(FCC_EngagementGroup& Group, float GroupDeltaTime, float Now);
	void AssignSlots(FCC_EngagementGroup& Group, float Now);
	void TryGrantToken(FCC_EngagementGroup& Group, float Now);

	/** 收回令牌：档位回到 Surround，编号 +1；bCooldownSlot 为 true 时该内圈槽位进入冷却。 */
	void RevokeToken(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry, bool bCooldownSlot, float Now);
	/** 给刚失去令牌的攻击者在外圈 / 远程圈找一个空位，没有则排队。 */
	void PlaceInSurroundRing(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry) const;

	FCC_EngagementAssignment BuildAssignment(const FCC_EngagementGroup& Group, const FCC_AttackerEntry& Entry) const;
	void NotifyIfChanged(FCC_EngagementGroup& Group, FCC_AttackerEntry& Entry, bool bForce);
	void FlushNotifies();

	float ComputeRangedRadius(const AActor* Attacker) const;
	static float GetAttackDelay(const AActor* Attacker);
	static bool IsAttacking(const AActor* Attacker);

	void DrawDebug(const FCC_EngagementGroup& Group) const;
};
