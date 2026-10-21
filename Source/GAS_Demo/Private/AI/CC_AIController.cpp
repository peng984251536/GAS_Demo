// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/CC_AIController.h"

#include "GAS_Demo.h"
#include "AbilitySystemComponent.h"
#include "AI/CC_CombatRangeStatics.h"
#include "AI/Crowd/CC_CrowdAgentStatics.h"
#include "AI/Engagement/CC_EngagementSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Data/CC_CharacterConfig.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/CC_Tags.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Utils/Team/CC_TeamStatics.h"
#include "TimerManager.h"

namespace
{
	/** 行为树读取的黑板键名，与黑板资产保持一致。 */
	const FName TargetActorKey(TEXT("TargetActor"));
	const FName LastKnownLocationKey(TEXT("LastKnownLocation"));
	/** 群体战斗写入的黑板键名（见 Docs/AI-Combat.zh-CN.md 第 7 节）。键不存在时写入静默失败。 */
	const FName IsBlockedKey(TEXT("IsBlocked"));
	const FName SquadOrderKey(TEXT("SquadOrder"));
}

#pragma region
// Sets default values
ACC_AIController::ACC_AIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	// 1. 创建感知组件
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception Component"));
	// 官方强调：必须指定给 AIController
	SetPerceptionComponent(*PerceptionComponent);

	// 2. 创建视觉配置并注册
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
}

void ACC_AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	BaseCharacter = Cast<ACC_BaseCharacter>(InPawn);
	if (!ensureMsgf(BaseCharacter,
		TEXT("ACC_AIController 附着到了非 CC_BaseCharacter 的 Pawn: %s"), *GetNameSafe(InPawn)))
	{
		return;
	}

	// 避障默认参数与 Attack 标签事件不依赖角色配置，先于配置检查完成。
	UCC_CrowdAgentStatics::ConfigureCrowdAgent(this);
	BindEngagementEvents();

	const UCC_CharacterConfig* CharacterConfig = BaseCharacter->GetCharacterConfig();
	if (!CharacterConfig)
	{
		return;
	}

	// 依赖角色的视野范围必须在附着后才能设置，构造函数里拿不到 Pawn
	SightConfig->PeripheralVisionAngleDegrees = 270.f;
	// 官方文档明确：这是"相对前向的半角"，不是总视角

	// 敌 / 中 / 友全部检测，敌我在 IsValidTarget 里用 UCC_TeamStatics 过滤。
	// 不依赖感知层的阵营过滤：魅惑会在运行时改变敌我关系，感知层不会自动刷新。
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	SightConfig->SightRadius = CharacterConfig->MaxRange;
	// 必须 > SightRadius（滞回，防边界抖动）
	SightConfig->LoseSightRadius = CharacterConfig->MaxRange + 200;
	// 主导感官决定"感知到的目标位置"
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	PerceptionComponent->RequestStimuliListenerUpdate();

	// FActorPerceptionUpdateDelegate(AActor*,FAIStimulus)：某个 Actor 的感知状态变化时通知
	PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&ThisClass::OnTargetPerceptionUpdated);

	// 触发 2：自己被魅惑 / 解除魅惑时立即重新选目标
	BindCharmedTagEvent();
	// 触发 3：低频重新选目标，处理别人被魅惑、目标死亡等没有感知事件的变化
	GetWorldTimerManager().SetTimer(ReevaluateTimerHandle, this, &ThisClass::ReevaluateTarget,
		TargetReevaluateInterval, true);

	//RunBehaviorTree()
	// 如果 OnPossess 发生在 BeginPlay 之后，立即刷新是安全的
	if (HasActorBegunPlay())
	{
		PerceptionComponent->RequestStimuliListenerUpdate();
	}
}

void ACC_AIController::OnUnPossess()
{
	if (IsValid(PerceptionComponent))
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
			this,
			&ThisClass::OnTargetPerceptionUpdated);
	}
	// 关卡卸载时 World 可能已不可用，不能直接用 GetWorldTimerManager()
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReevaluateTimerHandle);
	}
	UnbindCharmedTagEvent();
	CurrentTarget.Reset();

	// 从登记表注销，清空被挡状态（不写黑板：行为树随 Pawn 一起停止）。
	UpdateEngagement(nullptr);
	UnbindEngagementEvents();
	ResetBlockedProgress();
	BlockedTarget.Reset();
	bBlockedWaiting = false;
	AvoidanceLOD = -1;

	BaseCharacter = nullptr;
	Super::OnUnPossess();
}

// Called when the game starts or when spawned
void ACC_AIController::BeginPlay()
{
	Super::BeginPlay();

	// 处理 OnPossess 发生在 BeginPlay 之前的情况
	if (IsValid(PerceptionComponent) &&
		PerceptionComponent->IsRegistered())
	{
		PerceptionComponent->RequestStimuliListenerUpdate();
	}
}

#pragma endregion

#pragma region 选目标

/**
 * 感知更新回调。这里不直接决定目标，只做一件事：
 * 如果离开视野的正好是当前目标，记下它的最后位置，交给 ApplyTarget 写入 LastKnownLocation。
 * 离开视野的是其他 Actor 时不影响当前目标（旧实现会在此清空目标）。
 * @param Actor 感知状态发生变化的 Actor。
 * @param Stimulus 本次刺激；WasSuccessfullySensed 为 false 表示离开视野，StimulusLocation 为最后位置。
 */
void ACC_AIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsValid(Actor) || !IsValid(BaseCharacter))
	{
		return;
	}

	const bool bLostCurrentTarget = !Stimulus.WasSuccessfullySensed() && Actor == CurrentTarget.Get();
	UE_LOG(LogGAS_Demo, Verbose,
		TEXT("[AI][Targeting] Perception: Controller=%s Actor=%s Sensed=%d LostCurrent=%d"),
		*GetNameSafe(this), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed(), bLostCurrentTarget);

	const FVector LostLocation = Stimulus.StimulusLocation;
	ReevaluateTargetInternal(bLostCurrentTarget ? &LostLocation : nullptr);
}

void ACC_AIController::ReevaluateTarget()
{
	// 被挡检测在重选之前：判定被挡时当前目标会被加上惩罚，这次重选就有机会换到别的目标。
	const bool bJustBlocked = UpdateBlockedState();
	const AActor* BlockedBefore = bJustBlocked ? CurrentTarget.Get() : nullptr;

	ReevaluateTargetInternal(nullptr);

	AActor* Target = CurrentTarget.Get();
	const UWorld* World = GetWorld();
	if (bJustBlocked && Target && Target == BlockedBefore)
	{
		// 附近没有更好的目标：原地等一会儿再试，不往人堆里硬挤。
		SetBlockedWaiting(true);
	}
	else if (bBlockedWaiting && World && World->GetTimeSeconds() >= BlockedWaitUntil)
	{
		// 等待结束，让行为树重新尝试靠近；仍挤不进去时 BlockedTimeout 后会再次判定。
		SetBlockedWaiting(false);
	}

	// 自愈：目标没变时 ApplyTarget 不会再调用，若登记表已把自己清理掉（例如短暂判定为非敌对），
	// 这里重新登记。已登记同一目标时 RegisterAttacker 只查一次表。
	if (Target)
	{
		UpdateEngagement(Target);
	}

	UpdateAvoidanceLOD();
	DrawBlockedDebug();
}

/**
 * 选目标的实际实现：取视野内候选 → 构建评分候选（拥挤、回敬、被挡）→ FCC_TargetScoring → 有变化才 ApplyTarget。
 * 黑板尚未就绪时（行为树还没运行）不修改 CurrentTarget，等下一次触发再试，
 * 否则 CurrentTarget 已更新而黑板没写入，之后会因"目标没变"一直跳过。
 * @param LostLocation 当前目标刚离开视野时的最后位置；其他情况为 nullptr。
 */
void ACC_AIController::ReevaluateTargetInternal(const FVector* LostLocation)
{
	if (!IsValid(BaseCharacter) || !IsValid(PerceptionComponent))
	{
		return;
	}
	if (!IsValid(GetBlackboardComponent()))
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Targeting] Blackboard not ready: Controller=%s"), *GetNameSafe(this));
		return;
	}

	TArray<AActor*> Perceived;
	PerceptionComponent->GetCurrentlyPerceivedActors(nullptr, Perceived);

	ACC_BaseCharacter* Self = BaseCharacter.Get();
	const UCC_EngagementSubsystem* Registry = GetEngagementSubsystem();
	const bool bSelfIsMelee = !UCC_CombatRangeStatics::IsRangedCharacter(Self);
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const AActor* Blocked = Now < BlockedUntil ? BlockedTarget.Get() : nullptr;
	const FVector SelfLocation = Self->GetActorLocation();
	AActor* Current = CurrentTarget.Get();

	TArray<FCC_TargetCandidate, TInlineAllocator<16>> Candidates;
	int32 CurrentIndex = INDEX_NONE;
	for (AActor* Actor : Perceived)
	{
		if (!IsValidTarget(Self, Actor) || !PassesSquadConstraint(Actor))
		{
			continue;
		}
		if (Actor == Current)
		{
			CurrentIndex = Candidates.Num();
		}
		FCC_TargetCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Actor = Actor;
		Candidate.Distance = FVector::Dist2D(SelfLocation, Actor->GetActorLocation());
		Candidate.bBlocked = Actor == Blocked;
		if (Registry)
		{
			Candidate.MeleeAttackers = Registry->GetMeleeAttackerCount(Actor, Self);
			Candidate.bTargetsSelf = Registry->GetTargetOf(Actor) == Self;
		}
	}

	const int32 BestIndex = FCC_TargetScoring::SelectBest(Candidates, CurrentIndex, TargetScoring,
		TargetSwitchHysteresis, bSelfIsMelee);
	AActor* Best = Candidates.IsValidIndex(BestIndex) ? Candidates[BestIndex].Actor : nullptr;
	if (Best == Current)
	{
		return;
	}
	ApplyTarget(Best, Best ? nullptr : LostLocation);
}

/**
 * 判断候选是否可以作为目标。阵营由 UCC_TeamStatics 决定，已包含魅惑等临时状态。
 * @param Self 发起索敌的角色。
 * @param Candidate 候选 Actor。
 * @return 候选是存活的 ACC_BaseCharacter 且与 Self 敌对时返回 true。
 */
bool ACC_AIController::IsValidTarget(const AActor* Self, const AActor* Candidate)
{
	const ACC_BaseCharacter* Target = Cast<ACC_BaseCharacter>(Candidate);
	return IsValid(Target) && Target->IsAlive() && UCC_TeamStatics::IsHostile(Self, Target);
}

/**
 * 只按距离选目标（不考虑拥挤、回敬、被挡），并对当前目标做迟滞保护。
 * 与控制器内部使用同一个 FCC_TargetScoring，只是候选只填距离；供自动化测试与不需要群体信息的调用方使用。
 * @param Self 发起索敌的角色。
 * @param Candidates 视野内的候选。
 * @param Current 当前目标，可为空。
 * @param SwitchHysteresis 新目标需要比当前目标近多少（厘米）才切换。
 * @return 应锁定的目标；没有有效候选时返回 nullptr。
 */
AActor* ACC_AIController::SelectBestTarget(const AActor* Self, const TArray<AActor*>& Candidates,
	AActor* Current, float SwitchHysteresis)
{
	if (!IsValid(Self))
	{
		return nullptr;
	}
	const FVector SelfLocation = Self->GetActorLocation();

	TArray<FCC_TargetCandidate, TInlineAllocator<16>> Valid;
	int32 CurrentIndex = INDEX_NONE;
	for (AActor* Candidate : Candidates)
	{
		if (!IsValidTarget(Self, Candidate))
		{
			continue;
		}
		if (Candidate == Current)
		{
			CurrentIndex = Valid.Num();
		}
		FCC_TargetCandidate& Entry = Valid.AddDefaulted_GetRef();
		Entry.Actor = Candidate;
		Entry.Distance = FVector::Dist2D(SelfLocation, Candidate->GetActorLocation());
	}

	const int32 BestIndex = FCC_TargetScoring::SelectBest(Valid, CurrentIndex, FCC_TargetScoringParams(),
		SwitchHysteresis, true);
	return Valid.IsValidIndex(BestIndex) ? Valid[BestIndex].Actor : nullptr;
}

/**
 * 唯一写入目标的地方。行为树读黑板，技能读 GetClosestActor，两者在这里同步更新。
 * @param NewTarget 新目标；为空表示清空目标。
 * @param LostLocation 清空目标且原因是当前目标离开视野时，传入最后位置，行为树据此进入搜索；
 *        目标死亡或变成友方时传 nullptr，同时清空 LastKnownLocation，不去尸体或友方处搜索。
 */
void ACC_AIController::ApplyTarget(AActor* NewTarget, const FVector* LostLocation)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp) || !IsValid(BaseCharacter))
	{
		return;
	}
	CurrentTarget = NewTarget;
	// 换目标后被挡检测重新开始；新目标还没尝试过靠近，不需要等待。
	ResetBlockedProgress();
	SetBlockedWaiting(false);

	if (IsValid(NewTarget))
	{
		// 看到目标 → 写入 Blackboard，行为树进入追击分支
		const FVector TargetLocation = NewTarget->GetActorLocation();
		BlackboardComp->SetValueAsObject(TargetActorKey, NewTarget);
		BlackboardComp->SetValueAsVector(LastKnownLocationKey, TargetLocation);
		BaseCharacter->SetClosestActor(FClosestActorWithTagResult(NewTarget, TargetLocation));
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Targeting] Target set: Controller=%s Target=%s"),
			*GetNameSafe(this), *GetNameSafe(NewTarget));
	}
	else if (LostLocation)
	{
		// 当前目标离开视野 → 清空目标但保留最后位置，让 AI 走过去查看
		BlackboardComp->ClearValue(TargetActorKey);
		BlackboardComp->SetValueAsVector(LastKnownLocationKey, *LostLocation);
		BaseCharacter->SetClosestActor(FClosestActorWithTagResult(nullptr, *LostLocation));
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Targeting] Target lost, searching: Controller=%s Location=%s"),
			*GetNameSafe(this), *LostLocation->ToCompactString());
	}
	else
	{
		// 目标死亡 / 变成友方 / 被销毁 → 完全清空，不进入搜索
		BlackboardComp->ClearValue(TargetActorKey);
		BlackboardComp->ClearValue(LastKnownLocationKey);
		BaseCharacter->SetClosestActor(FClosestActorWithTagResult());
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Targeting] Target cleared: Controller=%s"), *GetNameSafe(this));
	}

	// 目标变化同步到登记表：魅惑导致的换目标也走这里。
	UpdateEngagement(IsValid(NewTarget) ? NewTarget : nullptr);
}

/**
 * 自己的魅惑标签计数变化：敌我关系整体翻转，立即重新选目标，不等计时器。
 * @param Tag 变化的标签（CCTags.Status.Charmed）。
 * @param NewCount 变化后的计数；大于 0 表示正处于魅惑状态。
 */
void ACC_AIController::OnCharmedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Targeting] Charmed changed: Controller=%s Count=%d"),
		*GetNameSafe(this), NewCount);
	ReevaluateTarget();
}

void ACC_AIController::BindCharmedTagEvent()
{
	UnbindCharmedTagEvent();
	UAbilitySystemComponent* ASC = IsValid(BaseCharacter) ? BaseCharacter->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC))
	{
		return;
	}
	CharmedTagHandle = ASC->RegisterGameplayTagEvent(CCTags::Status::Charmed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnCharmedTagChanged);
	CharmedTagASC = ASC;
}

void ACC_AIController::UnbindCharmedTagEvent()
{
	if (UAbilitySystemComponent* ASC = CharmedTagASC.Get())
	{
		ASC->RegisterGameplayTagEvent(CCTags::Status::Charmed, EGameplayTagEventType::NewOrRemoved)
			.Remove(CharmedTagHandle);
	}
	CharmedTagHandle.Reset();
	CharmedTagASC.Reset();
}

#pragma endregion

#pragma region 群体战斗

UCC_EngagementSubsystem* ACC_AIController::GetEngagementSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UCC_EngagementSubsystem>() : nullptr;
}

/**
 * 目标变化后同步登记表。弓兵按是否配置了箭判断，与 CC_AttackIntend 的派发规则一致。
 * @param NewTarget 新目标；为空表示注销。
 */
void ACC_AIController::UpdateEngagement(AActor* NewTarget)
{
	UCC_EngagementSubsystem* Subsystem = GetEngagementSubsystem();
	if (!Subsystem || !IsValid(BaseCharacter))
	{
		return;
	}
	if (!IsValid(NewTarget))
	{
		Subsystem->UnregisterAttacker(BaseCharacter);
		return;
	}
	Subsystem->RegisterAttacker(BaseCharacter, NewTarget, UCC_CombatRangeStatics::IsRangedCharacter(BaseCharacter));
}

/**
 * 编队约束挂点。v3 不实现编队行为，所有候选都满足。
 * 实现编队层时在这里按 SquadOrder 过滤：
 *   Follow / Hold：只接受离锚点（或坚守位置）EngageRadius 以内的敌人；
 *   Retreat：不接受任何目标。
 */
bool ACC_AIController::PassesSquadConstraint(const AActor* /*Candidate*/) const
{
	return true;
}

void ACC_AIController::SetSquadOrder(const FCC_SquadOrder& NewOrder)
{
	SquadOrder = NewOrder;
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsEnum(SquadOrderKey, static_cast<uint8>(NewOrder.Order));
	}

	const bool bImplemented = NewOrder.Order == ECC_SquadOrder::Free || NewOrder.Order == ECC_SquadOrder::Charge;
	if (bImplemented)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Squad] Order: Controller=%s Order=%s"),
			*GetNameSafe(this), *UEnum::GetValueAsString(NewOrder.Order));
	}
	else
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][Squad] Order %s is not implemented yet, treated as Free: Controller=%s"),
			*UEnum::GetValueAsString(NewOrder.Order), *GetNameSafe(this));
	}
	ReevaluateTarget();
}

/**
 * 被挡检测，只对近战、有目标、未在攻击的单位。每次重选目标之前调用（0.5 秒计时器、魅惑事件等）。
 *   - 已在攻击距离内，或边缘距离比最近记录缩短 BlockedProgressStep 以上：有进展，清零；
 *   - 目标在快速移动：追逃，不计时；
 *   - 其他情况累计，超过 BlockedTimeout 判定为被挡：记下被挡目标，BlockedMemory 秒内选目标时加惩罚。
 * @return 本次判定为被挡。
 */
bool ACC_AIController::UpdateBlockedState()
{
	const UWorld* World = GetWorld();
	AActor* Target = CurrentTarget.Get();
	if (!World || !IsValid(BaseCharacter) || !IsValid(Target) || bAttackTagActive
		|| UCC_CombatRangeStatics::IsRangedCharacter(BaseCharacter))
	{
		ResetBlockedProgress();
		SetBlockedWaiting(false);
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (ProgressTarget.Get() != Target)
	{
		ResetBlockedProgress();
		ProgressTarget = Target;
	}
	const float DeltaTime = LastProgressCheckTime < 0.f ? 0.f : FMath::Clamp(Now - LastProgressCheckTime, 0.f, 1.f);
	LastProgressCheckTime = Now;

	const float EdgeDistance = UCC_CombatRangeStatics::GetEdgeDistance2D(BaseCharacter, Target);
	if (EdgeDistance <= UCC_CombatRangeStatics::GetMeleeReach(BaseCharacter)
		|| EdgeDistance < BestEdgeDistance - BlockedProgressStep)
	{
		BestEdgeDistance = EdgeDistance;
		NoProgressTime = 0.f;
		SetBlockedWaiting(false);
		return false;
	}
	if (Target->GetVelocity().Size2D() > TargetFastSpeed)
	{
		return false;
	}

	NoProgressTime += DeltaTime;
	if (NoProgressTime < BlockedTimeout)
	{
		return false;
	}

	NoProgressTime = 0.f;
	BestEdgeDistance = EdgeDistance;
	BlockedTarget = Target;
	BlockedUntil = Now + BlockedMemory;
	UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Blocked] Blocked: Controller=%s Target=%s EdgeDistance=%.0f"),
		*GetNameSafe(this), *GetNameSafe(Target), EdgeDistance);
	return true;
}

void ACC_AIController::ResetBlockedProgress()
{
	ProgressTarget.Reset();
	BestEdgeDistance = TNumericLimits<float>::Max();
	NoProgressTime = 0.f;
	LastProgressCheckTime = -1.f;
}

void ACC_AIController::SetBlockedWaiting(bool bBlocked)
{
	if (bBlockedWaiting == bBlocked)
	{
		return;
	}
	bBlockedWaiting = bBlocked;
	if (bBlocked)
	{
		const UWorld* World = GetWorld();
		BlockedWaitUntil = (World ? World->GetTimeSeconds() : 0.f) + BlockedWaitTime;
		// 停下当前移动：FollowTarget 随之结束，行为树进入 IsBlocked 等待分支。
		StopMovement();
	}
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsBool(IsBlockedKey, bBlocked);
	}
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Blocked] Waiting=%d: Controller=%s"), bBlocked, *GetNameSafe(this));
}

void ACC_AIController::UpdateAvoidanceLOD()
{
	const UWorld* World = GetWorld();
	if (!World || !IsValid(BaseCharacter))
	{
		return;
	}
	const FVector SelfLocation = BaseCharacter->GetActorLocation();
	float NearestDistSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (PlayerPawn)
		{
			NearestDistSq = FMath::Min(NearestDistSq, FVector::DistSquared(SelfLocation, PlayerPawn->GetActorLocation()));
		}
	}
	const float NearestDistance = NearestDistSq < TNumericLimits<float>::Max()
		? FMath::Sqrt(NearestDistSq)
		: TNumericLimits<float>::Max();
	AvoidanceLOD = UCC_CrowdAgentStatics::UpdateAvoidanceLOD(this, NearestDistance, AvoidanceLOD,
		HighAvoidanceDistance, MediumAvoidanceDistance);
}

void ACC_AIController::DrawBlockedDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (!bBlockedWaiting || !IsValid(BaseCharacter) || !UCC_EngagementSubsystem::IsDebugDrawEnabled())
	{
		return;
	}
	DrawDebugString(GetWorld(), BaseCharacter->GetActorLocation() + FVector(0.f, 0.f, 120.f), TEXT("Blocked"),
		nullptr, FColor::Orange, TargetReevaluateInterval);
#endif
}

/**
 * Attack 标签计数变化。攻击能力（UCC_Primary / UCC_BowShoot）通过 ActivationOwnedTags 授予该标签，
 * 能力以任何方式结束（播完、打断、取消、死亡）都会移除它；连段期间计数不会降到 0。
 * 攻击期间切 ObstacleOnly：自己站稳不被推，后到的同伴绕开或被挤到侧面。
 */
void ACC_AIController::OnAttackTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		if (bAttackTagActive)
		{
			return;
		}
		bAttackTagActive = true;
		ResetBlockedProgress();
		SetBlockedWaiting(false);
		UCC_CrowdAgentStatics::SetAttacking(this, true);
		return;
	}

	if (!bAttackTagActive)
	{
		return;
	}
	bAttackTagActive = false;
	UCC_CrowdAgentStatics::SetAttacking(this, false);
}

void ACC_AIController::BindEngagementEvents()
{
	UnbindEngagementEvents();

	UAbilitySystemComponent* ASC = IsValid(BaseCharacter) ? BaseCharacter->GetAbilitySystemComponent() : nullptr;
	if (IsValid(ASC))
	{
		AttackTagHandle = ASC->RegisterGameplayTagEvent(CCTags::CCAbilities::Attack, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnAttackTagChanged);
		AttackTagASC = ASC;
	}
	bAttackTagActive = false;
}

void ACC_AIController::UnbindEngagementEvents()
{
	if (UAbilitySystemComponent* ASC = AttackTagASC.Get())
	{
		ASC->RegisterGameplayTagEvent(CCTags::CCAbilities::Attack, EGameplayTagEventType::NewOrRemoved)
			.Remove(AttackTagHandle);
	}
	AttackTagHandle.Reset();
	AttackTagASC.Reset();
	bAttackTagActive = false;
}

#pragma endregion
