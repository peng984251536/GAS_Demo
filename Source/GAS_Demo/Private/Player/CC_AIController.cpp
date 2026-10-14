// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CC_AIController.h"

#include "GAS_Demo.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "Team/CC_TeamStatics.h"
#include "TimerManager.h"

namespace
{
	/** 行为树读取的黑板键名，与黑板资产保持一致。 */
	const FName TargetActorKey(TEXT("TargetActor"));
	const FName LastKnownLocationKey(TEXT("LastKnownLocation"));
}

#pragma region
// Sets default values
ACC_AIController::ACC_AIController()
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
