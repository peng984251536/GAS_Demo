// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "CC_AIController.generated.h"

class UAbilitySystemComponent;
class UCC_EngagementSubsystem;
struct FCC_EngagementAssignment;

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
 * 围攻与避障（见 Docs/AI-Engagement.zh-CN.md）：
 *   - 寻路组件为 UCrowdFollowingComponent（Detour Crowd 避障）；
 *   - ApplyTarget 中向 UCC_EngagementSubsystem 注册 / 注销，分配变化写入黑板
 *     EngagementTier / MoveLocation / HasAttackToken，并按档位调整避障质量；
 *   - 监听自己 ASC 的 CCAbilities.Attack 标签：0→1 记下分配编号并切 ObstacleOnly，
 *     1→0 恢复避障并带编号归还令牌。
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

	/** 切换目标所需的最小距离优势（厘米）：新目标必须比当前目标近这么多才切换。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float TargetSwitchHysteresis = 200.f;

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
	 * 目标变化后同步围攻注册：有目标时注册（同一目标不重复），无目标时注销。
	 * @param NewTarget 新目标；为空表示注销。
	 */
	void UpdateEngagement(AActor* NewTarget);

	/**
	 * 子系统的分配变化回调：只处理自己 Pawn 的分配，写黑板并按档位调整避障质量。
	 * @param Attacker 分配变化的攻击者。
	 * @param Assignment 新分配。
	 */
	void OnEngagementAssignmentChanged(AActor* Attacker, const FCC_EngagementAssignment& Assignment);

	/**
	 * 自己的 CCAbilities.Attack 标签计数变化（仅 0↔1 时触发）。
	 * @param Tag 变化的标签。
	 * @param NewCount 变化后的计数；大于 0 表示开始攻击，0 表示攻击（含整套连段）结束或被打断。
	 */
	void OnAttackTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 绑定 / 解绑围攻相关事件：子系统的分配变化、自己 ASC 的 Attack 标签。 */
	void BindEngagementEvents();
	void UnbindEngagementEvents();

	/** 当前目标。弱引用，目标被销毁后自动失效。 */
	TWeakObjectPtr<AActor> CurrentTarget;
	/** 低频重新选目标的计时器。 */
	FTimerHandle ReevaluateTimerHandle;
	/** 魅惑标签事件的订阅句柄及其所在 ASC，解绑时使用。 */
	FDelegateHandle CharmedTagHandle;
	TWeakObjectPtr<UAbilitySystemComponent> CharmedTagASC;

	/** 围攻子系统的分配变化订阅。 */
	FDelegateHandle AssignmentChangedHandle;
	TWeakObjectPtr<UCC_EngagementSubsystem> EngagementSubsystem;
	/** Attack 标签事件的订阅句柄及其所在 ASC。 */
	FDelegateHandle AttackTagHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AttackTagASC;
	/** 本次攻击开始（Attack 标签 0→1）时的分配编号，归还令牌时带上。 */
	int32 HeldGeneration = 0;
	/** 是否处于攻击中（已收到 0→1 且尚未收到 1→0）。 */
	bool bAttackTagActive = false;
};
