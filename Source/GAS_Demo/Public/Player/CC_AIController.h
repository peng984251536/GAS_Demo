// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Character/CC_BaseCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "CC_AIController.generated.h"


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


UCLASS()
class GAS_DEMO_API ACC_AIController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACC_AIController();

	// 当 Controller 附身到 Pawn 时调用：
	// 在这里启动行为树、初始化黑板
	virtual void OnPossess(APawn* InPawn) override;
	// 当 Controller 与 Pawn 解除绑定时调用：
	// 在这里解绑感知事件，避免野指针回调
	virtual void OnUnPossess() override;

	

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

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
