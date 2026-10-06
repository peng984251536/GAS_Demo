// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CC_AIController.h"

#include "GAS_Demo.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Data/CC_CharacterConfig.h"

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
	
	//SightConfig->DetectionByAffiliation.bDetectEnemies = false;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	SightConfig->SightRadius = CharacterConfig->MaxRange;
	SightConfig->LoseSightRadius = CharacterConfig->MaxRange + 200;
	// 主导感官决定"感知到的目标位置"
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	// 必须 > SightRadius（滞回，防边界抖动）
	PerceptionComponent->RequestStimuliListenerUpdate();

	// FPerceptionUpdatedDelegate（TArray<AActor*>） 每帧批量通知，告诉谁变了
	// FActorPerceptionUpdateDelegate(AActor*,FAIStimulus)
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&ThisClass::OnTargetPerceptionUpdated);

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


void ACC_AIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Perception] Blackboard invalid: Controller=%s Actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Actor));
		return;
	}
	// 按 GameplayTag 过滤（和你项目的 GAS 标签体系天然契合）
	if (!IsValid(Actor) || (!Actor->ActorHasTag(CrashTags::Player) && !Actor->ActorHasTag(CrashTags::Follower)))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Perception] CrashTags invalid: Controller=%s Actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Actor));
		return;
	}
	ACC_BaseCharacter* character = Cast<ACC_BaseCharacter>(Actor);
	if(!character)
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Perception] ACC_BaseCharacter invalid: Controller=%s Actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Actor));
		return;
	}
	if(!BaseCharacter)
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Perception] ACC_BaseCharacter invalid: selfActor=%s"),
			*GetNameSafe(BaseCharacter));
		return;
	}
	
	if (Stimulus.WasSuccessfullySensed())
	{
		// 看到目标 → 写入 Blackboard，行为树进入追击分支
		BlackboardComp->SetValueAsObject(TEXT("TargetActor"), Actor);
		BlackboardComp->SetValueAsVector(TEXT("LastKnownLocation"), Stimulus.StimulusLocation);
		//BlackboardComp->ClearValue(TEXT("LastKnownLocation"));
		//const FVector TargetDirection = (Stimulus.StimulusLocation - BaseCharacter->GetActorLocation());
		//BaseCharacter->SetLastMoveInputDirection(TargetDirection);

		const FClosestActorWithTagResult result(Actor, Stimulus.StimulusLocation);
		BaseCharacter->SetClosestActor(result);

		UE_LOG(LogGAS_Demo, Log,
			TEXT("[BowAI][Perception] ACC_BaseCharacter: Controller=%s Actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Actor));
	}
	else
	{
		// 目标丢失 → 不清除 Target，而是记最后目击点，让 AI 走过去查看
		BlackboardComp->ClearValue(TEXT("TargetActor"));
		// LastKnownLocation 是丢失目标后其的 位置
		BlackboardComp->SetValueAsVector(TEXT("LastKnownLocation"), Stimulus.StimulusLocation);
		// 是否彻底忘掉，交给 Max Age 或自己的计时器决定
		const FClosestActorWithTagResult result(nullptr, Stimulus.StimulusLocation);
		BaseCharacter->SetClosestActor(result);

		UE_LOG(LogGAS_Demo, Log,
			TEXT("[BowAI][Perception] loseTarget: Controller=%s Actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Actor));
	}

	// 感知组件的设计思路：
	// 目标离开视野后，刺激在MaxAge时间内保持“已知但不可见”的状态
	
}

// Called every frame
// void ACC_AIController::Tick(float DeltaTime)
// {
// 	Super::Tick(DeltaTime);
// }
