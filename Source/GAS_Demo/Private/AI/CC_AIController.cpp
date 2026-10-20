// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/CC_AIController.h"

#include "GAS_Demo.h"
#include "AbilitySystemComponent.h"
#include "AI/Crowd/CC_CrowdAgentStatics.h"
#include "AI/Engagement/CC_EngagementSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Utils/Team/CC_TeamStatics.h"
#include "TimerManager.h"

namespace
{
	/** 行为树读取的黑板键名，与黑板资产保持一致。 */
	const FName TargetActorKey(TEXT("TargetActor"));
	const FName LastKnownLocationKey(TEXT("LastKnownLocation"));
	/** 围攻分配写入的黑板键名（见 Docs/AI-Engagement-Usage.zh-CN.md）。 */
	const FName EngagementTierKey(TEXT("EngagementTier"));
	const FName MoveLocationKey(TEXT("MoveLocation"));
	const FName HasAttackTokenKey(TEXT("HasAttackToken"));
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

	// 避障默认参数与围攻事件不依赖角色配置，先于配置检查完成。
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

	// 先注销（归还令牌、释放槽位），再解绑；注销产生的 None 通知仍能写入黑板。
	UpdateEngagement(nullptr);
	UnbindEngagementEvents();

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
	ReevaluateTargetInternal(nullptr);

	// 自愈：目标没变时 ApplyTarget 不会再调用，若子系统已把自己清理出组（例如短暂判定为非敌对），
	// 这里重新注册。已在同一目标的组中时 RegisterAttacker 什么都不做，开销只有两次查表。
	if (AActor* Target = CurrentTarget.Get())
	{
		UpdateEngagement(Target);
	}
}

/**
 * 选目标的实际实现：取视野内候选 → SelectBestTarget → 有变化才 ApplyTarget。
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

	AActor* Best = SelectBestTarget(BaseCharacter, Perceived, CurrentTarget.Get(), TargetSwitchHysteresis);
	if (Best == CurrentTarget.Get())
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
 * 选出最近的有效目标，并对当前目标做迟滞保护。
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

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bCurrentStillValid = false;
	for (AActor* Candidate : Candidates)
	{
		if (!IsValidTarget(Self, Candidate))
		{
			continue;
		}
		if (Candidate == Current)
		{
			bCurrentStillValid = true;
		}
		const float DistSq = FVector::DistSquared(SelfLocation, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	// 当前目标仍在视野内且有效时，新目标的距离优势不足迟滞距离就不切换
	if (bCurrentStillValid && Best != Current)
	{
		const float CurrentDist = FVector::Dist(SelfLocation, Current->GetActorLocation());
		const float BestDist = FMath::Sqrt(BestDistSq);
		if (CurrentDist - BestDist < SwitchHysteresis)
		{
			return Current;
		}
	}
	return Best;
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

	// 目标变化同步到围攻子系统：魅惑导致的换目标也走这里。
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

#pragma region 围攻与避障

/**
 * 目标变化后同步围攻注册。弓兵按是否配置了箭判断，与 CC_AttackIntend 的派发规则一致。
 * @param NewTarget 新目标；为空表示注销。
 */
void ACC_AIController::UpdateEngagement(AActor* NewTarget)
{
	UCC_EngagementSubsystem* Subsystem = EngagementSubsystem.Get();
	if (!Subsystem)
	{
		const UWorld* World = GetWorld();
		Subsystem = World ? World->GetSubsystem<UCC_EngagementSubsystem>() : nullptr;
	}
	if (!Subsystem || !IsValid(BaseCharacter))
	{
		return;
	}
	if (!IsValid(NewTarget))
	{
		Subsystem->UnregisterAttacker(BaseCharacter);
		return;
	}

	
	const UCC_CharacterConfig* Config = BaseCharacter->GetCharacterConfig();
	const bool bRanged = IsValid(Config) && Config->ArrowProjectileClass != nullptr;
	// 刷新攻击目标后，注册进围攻组群 FCC_EngagementGroup
	Subsystem->RegisterAttacker(BaseCharacter, NewTarget, bRanged);
}

/**
 * 分配变化：写黑板（行为树据此选择分支），并按档位调整避障质量。
 * MoveLocation 变化超过子系统的 NotifyDistance 才会到这里，行为树可在该键变化时重新触发 FollowTarget。
 */
void ACC_AIController::OnEngagementAssignmentChanged(AActor* Attacker, const FCC_EngagementAssignment& Assignment)
{
	if (!Attacker || Attacker != GetPawn())
	{
		return;
	}
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsEnum(EngagementTierKey, static_cast<uint8>(Assignment.Tier));
		BlackboardComp->SetValueAsBool(HasAttackTokenKey, Assignment.bHasToken);
		if (Assignment.bLocationValid)
		{
			BlackboardComp->SetValueAsVector(MoveLocationKey, Assignment.MoveLocation);
		}
		else
		{
			BlackboardComp->ClearValue(MoveLocationKey);
		}
	}
	UCC_CrowdAgentStatics::ApplyTier(this, Assignment.Tier);
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Engagement] Assignment: Controller=%s Tier=%d Token=%d Valid=%d Generation=%d Location=%s"),
		*GetNameSafe(this), static_cast<int32>(Assignment.Tier), Assignment.bHasToken, Assignment.bLocationValid,
		Assignment.Generation, *Assignment.MoveLocation.ToCompactString());
}

/**
 * Attack 标签计数变化。攻击能力（UCC_Primary / UCC_BowShoot）通过 ActivationOwnedTags 授予该标签，
 * 能力以任何方式结束（播完、打断、取消、死亡）都会移除它；连段期间计数不会降到 0。
 * 弓兵没有令牌，1→0 时的归还会被子系统忽略。
 */
void ACC_AIController::OnAttackTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UCC_EngagementSubsystem* Subsystem = EngagementSubsystem.Get();
	if (NewCount > 0)
	{
		if (bAttackTagActive)
		{
			return;
		}
		bAttackTagActive = true;
		HeldGeneration = Subsystem ? Subsystem->GetAssignment(BaseCharacter).Generation : 0;
		UCC_CrowdAgentStatics::SetAttacking(this, true);
		return;
	}

	if (!bAttackTagActive)
	{
		return;
	}
	bAttackTagActive = false;
	// 先恢复避障（此时尚未开始新的移动），再归还令牌：归还会触发新的分配与移动。
	UCC_CrowdAgentStatics::SetAttacking(this, false);
	if (Subsystem && IsValid(BaseCharacter))
	{
		Subsystem->ReleaseToken(BaseCharacter, HeldGeneration);
	}
}

void ACC_AIController::BindEngagementEvents()
{
	UnbindEngagementEvents();

	if (UWorld* World = GetWorld())
	{
		if (UCC_EngagementSubsystem* Subsystem = World->GetSubsystem<UCC_EngagementSubsystem>())
		{
			AssignmentChangedHandle = Subsystem->OnAssignmentChanged.AddUObject(
				this, &ThisClass::OnEngagementAssignmentChanged);
			EngagementSubsystem = Subsystem;
		}
	}

	UAbilitySystemComponent* ASC = IsValid(BaseCharacter) ? BaseCharacter->GetAbilitySystemComponent() : nullptr;
	if (IsValid(ASC))
	{
		AttackTagHandle = ASC->RegisterGameplayTagEvent(CCTags::CCAbilities::Attack, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnAttackTagChanged);
		AttackTagASC = ASC;
	}
	bAttackTagActive = false;
	HeldGeneration = 0;
}

/**
 * 清除旧目标
 * 
 */
void ACC_AIController::UnbindEngagementEvents()
{
	// 移除订阅 AssignmentChangedHandle（围攻改变）
	if (UCC_EngagementSubsystem* Subsystem = EngagementSubsystem.Get())
	{
		Subsystem->OnAssignmentChanged.Remove(AssignmentChangedHandle);
	}
	AssignmentChangedHandle.Reset();
	EngagementSubsystem.Reset();

	if (UAbilitySystemComponent* ASC = AttackTagASC.Get())
	{
		ASC->RegisterGameplayTagEvent(CCTags::CCAbilities::Attack, EGameplayTagEventType::NewOrRemoved)
			.Remove(AttackTagHandle);
	}
	AttackTagHandle.Reset();
	AttackTagASC.Reset();
	bAttackTagActive = false;
	HeldGeneration = 0;
}

#pragma endregion
