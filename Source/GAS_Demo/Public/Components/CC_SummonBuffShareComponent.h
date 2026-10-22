#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CC_SummonBuffShareComponent.generated.h"

class ACC_BaseCharacter;
class UCC_BuffComponent;

/**
 * 召唤物身上的 Buff 共享（只在服务器存在，由 UCC_SummonAlly 生成召唤物后添加）。
 *
 * - 开始时复制召唤者当前的全部 Buff（按层数施加）。
 * - 之后召唤者每获得一次 Buff（新 Buff、叠层、刷新持续时间），召唤物也施加一次同样的 GE。
 * - 复制出来的 Buff 各自独立：按自己的持续时间结束，召唤者的 Buff 结束不影响召唤物。
 * - 带 Buff.NotShared 标签的 Buff 不同步（例如召唤 Buff 本身，否则召唤物会继续召唤）。
 * 只使用 UCC_BuffComponent 的公开接口（GetActiveBuffs / OnBuffsChanged / ApplyBuff），不判断具体 Buff。
 */
UCLASS(ClassGroup = (GAS))
class GAS_DEMO_API UCC_SummonBuffShareComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCC_SummonBuffShareComponent();

	/**
	 * 服务器：开始从召唤者同步 Buff。立即复制当前 Buff，并订阅之后的变化；EndPlay 时自动取消订阅。
	 * @param Summoner 召唤者。
	 */
	void StartSharing(ACC_BaseCharacter* Summoner);

	/** 召唤者；召唤者已销毁时为空。 */
	ACC_BaseCharacter* GetSummoner() const { return Summoner.Get(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleSummonerBuffsChanged();

	/** 对比召唤者 Buff 快照，把新增的施加次数复制给召唤物。 */
	void SyncNewApplications();

	struct FSeenBuff
	{
		int32 StackCount = 0;
		float EndServerTime = -1.f;
	};

	TWeakObjectPtr<ACC_BaseCharacter> Summoner;
	TWeakObjectPtr<UCC_BuffComponent> SummonerBuffs;
	/** 上一次看到的召唤者 Buff：InstanceId → 层数 / 到期时刻。 */
	TMap<int32, FSeenBuff> Seen;
};
