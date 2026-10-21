#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Data/CC_EnemySpawnTypes.h"
#include "CC_SummonAlly.generated.h"

class ACC_BaseCharacter;
class ACC_EnemyCharacter;
class ACC_GroundPlacementIndicator;
class ACC_TargetActor_GroundPlacement;
class UAbilityTask_WaitTargetData;

/**
 * 选点召唤随从：玩家用鼠标在地面选一个点，确认后在该处生成一个与施放者同阵营的 ACC_EnemyCharacter。
 *
 * 流程：
 *   激活 → WaitTargetData（UserConfirmed，本地生成 ACC_TargetActor_GroundPlacement + 地面高亮）
 *   → 攻击键确认 / 右键、Esc 或再按一次技能键取消
 *   → 客户端把落点发给服务器 → 服务器校验距离、投影 NavMesh → 生成随从 → 结束能力。
 *
 * 阵营：生成前调用 ACC_BaseCharacter::SetTeamOverride(施放者基础阵营)，敌我判断、AI 选目标、
 * 投射物命中、击杀经验全部通过 UCC_TeamStatics 自动生效，随从蓝图本身不需要任何改动。
 *
 * 激活方式：Asset Tag 为 CCTags.CCAbilities.Summon；在 InputConfig.AbilityInputActions 中把输入映射到该标签即可
 * （ACC_PlayerController 会按标签激活能力）。瞄准期间施放者带 CCTags.Status.Targeting。
 * 只支持有 PlayerController 的施放者。
 */
UCLASS()
class GAS_DEMO_API UCC_SummonAlly : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_SummonAlly();

	/**
	 * 服务器：校验落点并生成随从，成功后登记到 ActiveSummons 并执行数量上限。
	 * 一般由确认回调调用；公开是为了自动化测试。
	 * @param Caster 施放者，随从继承它的基础阵营。
	 * @param RequestedLocation 客户端提交的地面落点。
	 * @return 生成的随从；没有权限、超出距离、不在导航网格上或生成点被占用时返回 nullptr。
	 */
	ACC_EnemyCharacter* SpawnSummon(ACC_BaseCharacter* Caster, const FVector& RequestedLocation);

	/** 当前仍存活的随从（只在服务器上有内容）。 */
	UFUNCTION(BlueprintPure, Category = "Summon")
	TArray<ACC_EnemyCharacter*> GetActiveSummons() const;

protected:
	/** 召唤的敌人蓝图（ACC_EnemyCharacter 子类）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	TSubclassOf<ACC_EnemyCharacter> SummonClass;

	/** 施放者到落点的最大水平距离（厘米）；不大于 0 表示不限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float MaxRange = 1200.f;

	/** 服务器校验距离时的容差（厘米），抵消客户端与服务器位置差。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float RangeTolerance = 150.f;

	/** 同时存在的随从上限，超出时销毁最早的一只；0 表示不限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	int32 MaxActiveSummons = 3;

	/** 随从存在时间（秒），到时销毁；0 表示永久。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float SummonLifeSpan = 0.f;

	/** 能力被移除（例如授予它的 Buff 结束）时销毁全部随从，保持授予与收回对称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	bool bDestroySummonsOnAbilityRemoved = true;

	/** 勾选后用 SummonSpawnParams 初始化随从属性（GE、等级、血量倍率），否则沿用随从的 CharacterConfig。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	bool bOverrideSpawnParams = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (EditCondition = "bOverrideSpawnParams"))
	FCC_EnemySpawnParams SummonSpawnParams;

	/** 服务器把落点投影到导航网格的范围；场景没有导航数据时跳过投影。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	FVector NavProjectExtent = FVector(200.f, 200.f, 300.f);

	/** 选点用的 Target Actor；可换成蓝图子类调整按键、地面类型等。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon|Targeting")
	TSubclassOf<ACC_TargetActor_GroundPlacement> TargetActorClass;

	/** 地面高亮指示器；换成蓝图子类可指定贴花材质和颜色。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon|Targeting")
	TSubclassOf<ACC_GroundPlacementIndicator> IndicatorClass;

	/** 指示圈半径（厘米）；0 表示使用随从胶囊半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon|Targeting", meta = (ClampMin = "0"))
	float IndicatorRadius = 0.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

private:
	UFUNCTION()
	void OnPlacementConfirmed(const FGameplayAbilityTargetDataHandle& Data);
	UFUNCTION()
	void OnPlacementCancelled(const FGameplayAbilityTargetDataHandle& Data);

	void ConfigureTargetActor(ACC_TargetActor_GroundPlacement* TargetActor) const;
	/** 清掉已死亡 / 已销毁的随从，超出上限时销毁最早的。 */
	void EnforceSummonLimit();
	void DestroyAllSummons();
	void CancelActivation();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetTask;

	/** 本施放者召唤出的随从（运行时状态，按能力实例保存，只在服务器写入）。 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACC_EnemyCharacter>> ActiveSummons;
};
