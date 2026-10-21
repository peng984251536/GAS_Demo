#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CC_SurvivalTypes.generated.h"

class APlayerState;

/** 对局阶段（ACC_SurvivalGameState::Phase）。 */
UENUM(BlueprintType)
enum class ECC_MatchPhase : uint8
{
	/** 等待玩家就绪。 */
	WaitingToStart,
	/** 进行中（含开局预热和波次）。 */
	Playing,
	/** 全员阵亡，已结算。 */
	GameOver
};

/** 玩家生命状态（ACC_SurvivalPlayerState::LifeState）。全灭判定按 != Alive。 */
UENUM(BlueprintType)
enum class ECC_PlayerLifeState : uint8
{
	Alive,
	/** 生命已耗尽，死亡动作播放中。 */
	Dying,
	/** 死亡动作已结束，等待复活。 */
	Dead
};

/**
 * 客户端提示消息（横幅、升级提示等）。通过 GameState 的不可靠多播发送，丢失不影响正确性；
 * 需要可靠同步的状态一律走复制属性。参考 Lyra 的 Verb 消息。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_MatchMessage
{
	GENERATED_BODY()

	/** 消息类型，Survival.Msg.*。 */
	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	FGameplayTag Verb;

	/** 发起者（可空）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	TObjectPtr<APlayerState> Instigator = nullptr;

	/** 对象（可空），例如阵亡/复活的玩家。 */
	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	TObjectPtr<APlayerState> Target = nullptr;

	/** 附带数值，例如波次或等级。 */
	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	float Magnitude = 0.f;
};
