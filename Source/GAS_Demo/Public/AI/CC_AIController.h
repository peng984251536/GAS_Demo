// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AI/Squad/CC_SquadTypes.h"
#include "AI/Targeting/CC_TargetScoring.h"

#include "CC_AIController.generated.h"

class UAbilitySystemComponent;
class UCC_EngagementSubsystem;

// AI 的警戒等级枚举
// 用于驱动行为树的 Selector 分支切换（巡逻 / 警觉 / 战斗 / 搜索）
UENUM(BlueprintType)
enum class EAlertLevel : uint8
{
	Patrol      UMETA(DisplayName = "Patrol"),     // 正常巡逻，未发现任何目标
	//Suspicious  UMETA(DisplayName = "Suspicious"), // 察觉到一点异常（预留状态，可用于渐进式警觉）
	Alert       UMETA(DisplayName = "Alert"),      // 已确认发现目标，进入战斗/追击
	Searching   UMETA(DisplayName = "Searching")   // 目标丢失，正在前往最后已知位置搜索
};


/**
 * 敌人 AI 控制器：用视觉感知收集候选，按阵营选出当前目标，写入黑板和角色的 ClosestActor。
 *
 * 选目标只有一个入口 ReevaluateTarget，由三种时机触发：
 *   1. 感知事件（看见 / 看不见某个 Actor）；
 *   2. 自己的 CCTags.Status.Charmed 标签增减（自己被魅惑或解除）；
 *   3. 低频计时器（别人被魅惑、目标死亡等收不到通知的变化）。
 * 写入目标只有一个出口 ApplyTarget；行为树与技能只读黑板 / GetClosestActor。
 * 敌我判断全部交给 UCC_TeamStatics::IsHostile。
 *
 * 群体战斗（见 Docs/AI-Combat.zh-CN.md）——个体只锁目标、自由交战，包围由避障与推挤自然形成：
 *   - 选目标评分：距离 + 拥挤惩罚 − 回敬优惠 + 被挡惩罚（FCC_TargetScoring），带切换迟滞；
 *   - ApplyTarget 中向 UCC_EngagementSubsystem（攻击者登记表）登记「谁在打谁」，供别人计算拥挤与回敬；
 *   - 被挡检测（近战）：离目标不远但持续挤不进去时，惩罚该目标并重选；没有更好的目标时写黑板 IsBlocked，
 *     行为树原地等待，不硬挤；
 *   - 寻路组件为 UCrowdFollowingComponent；监听自己 ASC 的 CCAbilities.Attack 标签：
 *     攻击期间切 ObstacleOnly（站稳不被推，后到的人被挤到侧面），结束后恢复；
 *   - 避障质量按离最近玩家的距离分级；
 *   - 编队命令 SetSquadOrder：Follow 已实现（栓绳选目标 + 没有目标时写黑板 FollowAnchor，行为树跟随锚点，
 *     丢失目标时不去搜索），召唤的随从默认跟随施放者；Hold / Retreat 仍按自由交战处理。
 */
UCLASS()
class GAS_DEMO_API ACC_AIController : public AAIController
{
	GENERATED_BODY()

public:
	/** 用 UCrowdFollowingComponent 替换默认的 PathFollowingComponent。 */
	ACC_AIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 当 Controller 附身到 Pawn 时调用：
	// 在这里启动行为树、初始化黑板
	virtual void OnPossess(APawn* InPawn) override;
	// 当 Controller 与 Pawn 解除绑定时调用：
	// 在这里解绑感知事件，避免野指针回调
	virtual void OnUnPossess() override;

	/**
	 * 根据当前视野重新选择目标；目标没有变化时不写任何东西。
	 * 可从蓝图或 Buff 行为中调用，强制 AI 立即重新索敌。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Targeting")
	void ReevaluateTarget();

	/**
	 * @return 当前锁定的目标；没有目标或目标已被销毁时返回 nullptr。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/**
	 * 判断候选是否可以作为 Self 的目标：是存活的 ACC_BaseCharacter，且与 Self 敌对。
	 * @param Self 发起索敌的角色。
	 * @param Candidate 候选 Actor。
	 * @return 可以作为目标返回 true。
	 */
	static bool IsValidTarget(const AActor* Self, const AActor* Candidate);

	/**
	 * 从候选中选出最近的有效目标；当前目标仍有效且仍在候选中时，
	 * 只有新目标比它近超过 SwitchHysteresis 才切换，防止目标来回跳。
	 * 纯函数，不读写控制器状态，供 ReevaluateTarget 与自动化测试使用。
	 * @param Self 发起索敌的角色，距离以它的位置计算。
	 * @param Candidates 视野内的候选 Actor，可包含无效或友方 Actor。
	 * @param Current 当前目标，可为空。
	 * @param SwitchHysteresis 切换目标所需的最小距离优势（厘米）。
	 * @return 应锁定的目标；没有有效候选时返回 nullptr。
	 */
	static AActor* SelectBestTarget(const AActor* Self, const TArray<AActor*>& Candidates,
		AActor* Current, float SwitchHysteresis);

	/**
	 * 接收编队命令。保存命令、写入黑板 SquadOrder（键不存在时忽略）并立即重选目标。
	 * Follow：按 FCC_SquadOrder 的三个半径做栓绳，没有目标时写黑板 FollowAnchor；
	 * Hold / Retreat 尚未实现，会打印警告并按自由交战处理。
	 * @param NewOrder 新命令。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Squad")
	void SetSquadOrder(const FCC_SquadOrder& NewOrder);

	/** @return 当前编队命令；从未下达过时为 Free。 */
	UFUNCTION(BlueprintPure, Category = "AI|Squad")
	const FCC_SquadOrder& GetSquadOrder() const { return SquadOrder; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Component")
	//TObjectPtr<UAIPerceptionComponent> PerceptionComponent;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="Component")
	TObjectPtr<UAISenseConfig_Sight> SightConfig ;
	// 运行时由 OnPossess 赋值，指向当前附着的角色
	UPROPERTY(Transient)
	TObjectPtr<ACC_BaseCharacter> BaseCharacter ;

	/** 低频重新选目标的间隔（秒），用于发现别人被魅惑、目标死亡等没有感知事件的变化。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0.05"))
	float TargetReevaluateInterval = 0.5f;

	/** 切换目标所需的最小分数优势（只看距离时即厘米）：新目标的分数必须比当前目标低这么多才切换。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float TargetSwitchHysteresis = 200.f;

	/**
	 * 当前目标在这个水平距离（厘米）以内时，即使视觉这一刻没有感知到它，也继续作为候选，不丢失目标。
	 * 贴身近战时视线常被自己的武器、同伴或角度短暂挡住；没有这层保护时目标会被清空，怪物停止攻击，
	 * 直到目标重新进入视野。设为 0 关闭。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float KeepTargetRadius = 600.f;

	/** 选目标评分参数：拥挤软上限与惩罚、回敬优惠、被挡惩罚。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting")
	FCC_TargetScoringParams TargetScoring;

	/** 近战离目标不远、但持续这么久（秒）没有靠近，视为被人群挡住。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blocked", meta = (ClampMin = "0.1"))
	float BlockedTimeout = 1.5f;

	/** 被挡住的目标在这段时间（秒）内选目标时加 BlockedPenalty。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blocked", meta = (ClampMin = "0"))
	float BlockedMemory = 3.f;

	/** 没有更好的目标时，原地等待（黑板 IsBlocked = true）多久（秒）后再尝试靠近。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blocked", meta = (ClampMin = "0"))
	float BlockedWaitTime = 1.f;

	/** 边缘距离比最近记录缩短这么多（厘米）才算有进展。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blocked", meta = (ClampMin = "0"))
	float BlockedProgressStep = 25.f;

	/** 目标水平速度超过它（厘米/秒）时视为在逃跑，追不上不算被挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blocked", meta = (ClampMin = "0"))
	float TargetFastSpeed = 300.f;

	/** 离最近玩家小于这个距离（厘米）时避障质量为 High。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Crowd", meta = (ClampMin = "0"))
	float HighAvoidanceDistance = 1500.f;

	/** 离最近玩家小于这个距离（厘米）时避障质量为 Medium，更远为 Low。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Crowd", meta = (ClampMin = "0"))
	float MediumAvoidanceDistance = 4000.f;

	/**
	 * 感知更新回调：只记录"当前目标是否离开视野"，选目标交给 ReevaluateTarget。
	 * @param Actor 感知状态发生变化的 Actor。
	 * @param Stimulus 本次刺激；WasSuccessfullySensed 为 false 表示离开视野。
	 */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	/**
	 * 选目标的实际实现。
	 * @param LostLocation 当前目标刚离开视野时的最后位置；其他情况为 nullptr。
	 */
	void ReevaluateTargetInternal(const FVector* LostLocation);

	/**
	 * 唯一写入目标的地方：更新 CurrentTarget、黑板 TargetActor / LastKnownLocation 与角色 ClosestActor。
	 * @param NewTarget 新目标；为空表示清空目标。
	 * @param LostLocation 清空目标时，若因当前目标离开视野则传入最后位置，行为树据此进入搜索；
	 *        目标死亡或变成友方时传 nullptr，同时清空 LastKnownLocation，不去搜索。
	 */
	void ApplyTarget(AActor* NewTarget, const FVector* LostLocation);

	/**
	 * 自己的 CCTags.Status.Charmed 计数变化时立即重新选目标。
	 * @param Tag 变化的标签。
	 * @param NewCount 变化后的计数。
	 */
	void OnCharmedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 绑定 / 解绑自己 ASC 上的魅惑标签事件。 */
	void BindCharmedTagEvent();
	void UnbindCharmedTagEvent();

	/**
	 * 目标变化后同步登记表：有目标时登记（同一目标不重复），无目标时注销。
	 * @param NewTarget 新目标；为空表示注销。
	 */
	void UpdateEngagement(AActor* NewTarget);

	/** @return 攻击者登记表；子系统不存在时返回 nullptr。 */
	UCC_EngagementSubsystem* GetEngagementSubsystem() const;

	/**
	 * 编队约束：候选是否满足当前编队命令。Follow 时按栓绳过滤（FCC_SquadRules::PassesFollowLeash），
	 * 其他命令不过滤。以后实现 Hold / Retreat 也在这里加。
	 */
	bool PassesSquadConstraint(const AActor* Candidate) const;

	/**
	 * @return 跟随命令下的锚点；不是 Follow、锚点已销毁或已死亡时返回 nullptr。
	 */
	AActor* GetFollowAnchor() const;

	/** 同步黑板 FollowAnchor：跟随中且没有目标时写入锚点，否则清空。 */
	void UpdateFollowAnchorKey();

	/**
	 * 被挡检测（只对近战、有目标、未在攻击的单位）。在重选目标之前调用。
	 * @return 本次判定为被挡（已记录被挡目标），调用方重选后需要决定换目标还是原地等待。
	 */
	bool UpdateBlockedState();

	/** 清空被挡检测的进展记录。 */
	void ResetBlockedProgress();

	/**
	 * 设置黑板 IsBlocked。进入等待时停止当前移动，让 FollowTarget 结束、行为树进入等待分支。
	 * @param bBlocked 是否进入等待。
	 */
	void SetBlockedWaiting(bool bBlocked);

	/** 按离最近玩家的距离更新避障质量。 */
	void UpdateAvoidanceLOD();

	/** 调试绘制自己的被挡状态（cc.Engagement.Debug 1）。 */
	void DrawBlockedDebug() const;

	/**
	 * 自己的 CCAbilities.Attack 标签计数变化（仅 0↔1 时触发）。
	 * @param Tag 变化的标签。
	 * @param NewCount 变化后的计数；大于 0 表示开始攻击，0 表示攻击（含整套连段）结束或被打断。
	 */
	void OnAttackTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 绑定 / 解绑自己 ASC 的 Attack 标签事件。 */
	void BindEngagementEvents();
	void UnbindEngagementEvents();

	/** 当前目标。弱引用，目标被销毁后自动失效。 */
	TWeakObjectPtr<AActor> CurrentTarget;
	/** 低频重新选目标的计时器。 */
	FTimerHandle ReevaluateTimerHandle;
	/** 魅惑标签事件的订阅句柄及其所在 ASC，解绑时使用。 */
	FDelegateHandle CharmedTagHandle;
	TWeakObjectPtr<UAbilitySystemComponent> CharmedTagASC;

	/** Attack 标签事件的订阅句柄及其所在 ASC。 */
	FDelegateHandle AttackTagHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AttackTagASC;
	/** 是否处于攻击中（已收到 0→1 且尚未收到 1→0）。 */
	bool bAttackTagActive = false;

	/** 当前编队命令。 */
	UPROPERTY(Transient)
	FCC_SquadOrder SquadOrder;

	/** 被挡检测：正在统计进展的目标、最近的边缘距离、无进展累计时间、上次检测时间。 */
	TWeakObjectPtr<AActor> ProgressTarget;
	float BestEdgeDistance = TNumericLimits<float>::Max();
	float NoProgressTime = 0.f;
	float LastProgressCheckTime = -1.f;
	/** 最近被挡住的目标及惩罚截止时间。 */
	TWeakObjectPtr<AActor> BlockedTarget;
	float BlockedUntil = 0.f;
	/** 是否处于原地等待（黑板 IsBlocked）及等待截止时间。 */
	bool bBlockedWaiting = false;
	float BlockedWaitUntil = 0.f;

	/** 当前避障质量等级（0 Low / 1 Medium / 2 High），-1 表示尚未设置。 */
	int32 AvoidanceLOD = -1;
};
