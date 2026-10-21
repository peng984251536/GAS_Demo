#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "CC_EngagementSubsystem.generated.h"

class AActor;

/** 登记表中的一条记录：某个攻击者正在打谁。 */
struct FCC_EngagementRecord
{
	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Target;
	FObjectKey TargetKey;
	bool bRanged = false;
};

/**
 * 攻击者登记表：只回答「谁在打谁」，供选目标时计算拥挤程度和回敬。
 *
 * - 控制器在目标变化时注册 / 注销（ACC_AIController::ApplyTarget），并在 0.5 秒重选计时器中再注册一次自愈；
 * - 每 PruneInterval 秒清理一次已销毁、已死亡或不再敌对的记录，兜底控制器漏掉的注销；
 * - 不分配位置、不限制出手人数。站位由 Detour Crowd 的避让与推挤自然形成。
 *
 * 只在服务器上有数据（AIController 只存在于服务器），客户端上的实例 Tick 立即返回。
 * 设计见 Docs/AI-Combat.zh-CN.md。控制台变量 cc.Engagement.Debug 1 绘制攻击者 → 目标的连线。
 */
UCLASS()
class GAS_DEMO_API UCC_EngagementSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * 登记 Attacker 正在打 Target。已登记同一目标时只同步兵种；登记了别的目标时先移出。
	 * @param Attacker 攻击者。
	 * @param Target 目标；为空或等于 Attacker 时等同于注销。
	 * @param bRanged 是否为弓兵；弓兵不计入目标的近战人数。
	 */
	void RegisterAttacker(AActor* Attacker, AActor* Target, bool bRanged);

	/** 移除 Attacker 的登记。 */
	void UnregisterAttacker(AActor* Attacker);

	/**
	 * @param Target 目标。
	 * @param Exclude 不计入的攻击者，通常是查询者自己；可为空。
	 * @return 正在近战攻击 Target 的人数。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Engagement")
	int32 GetMeleeAttackerCount(const AActor* Target, const AActor* Exclude = nullptr) const;

	/**
	 * @return Attacker 当前登记的目标；没有登记或目标已销毁时返回 nullptr。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Engagement")
	AActor* GetTargetOf(const AActor* Attacker) const;

	/** @return 控制台变量 cc.Engagement.Debug 是否开启。控制器用它决定是否绘制自己的被挡状态。 */
	static bool IsDebugDrawEnabled();

private:
	/** 攻击者 → 记录。 */
	TMap<FObjectKey, FCC_EngagementRecord> Records;
	/** 目标 → 正在近战攻击它的攻击者。 */
	TMap<FObjectKey, TSet<FObjectKey>> MeleeByTarget;
	float TimeSincePrune = 0.f;

	/** 移除一条记录并同步 MeleeByTarget。 */
	void RemoveRecord(const FObjectKey& AttackerKey);
	/** 清理已销毁、已死亡、不再敌对的记录。 */
	void PruneInvalid();
	void DrawDebug() const;
};
