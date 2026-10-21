#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CC_GameEventSubsystem.generated.h"

class ACC_BaseCharacter;
class APlayerState;

/**
 * 一次角色死亡的只读通知（服务器）。
 * 由 ACC_BaseCharacter 在生命耗尽时广播一次；角色不知道谁在监听。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_DeathInfo
{
	GENERATED_BODY()

	/** 死亡的角色。 */
	UPROPERTY(BlueprintReadOnly, Category = "GameEvents")
	TObjectPtr<ACC_BaseCharacter> Victim = nullptr;

	/** 击杀者（来源角色）；非伤害导致的死亡、来源已销毁时为空。 */
	UPROPERTY(BlueprintReadOnly, Category = "GameEvents")
	TObjectPtr<AActor> Killer = nullptr;

	/** 直接造成伤害的 Actor（箭等），可能为空或等于 Killer。 */
	UPROPERTY(BlueprintReadOnly, Category = "GameEvents")
	TObjectPtr<AActor> DamageCauser = nullptr;

	/** 击杀者所属玩家；击杀者不是玩家控制的角色时为空。 */
	UPROPERTY(BlueprintReadOnly, Category = "GameEvents")
	TObjectPtr<APlayerState> KillerPlayerState = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FCC_OnCharacterDied, const FCC_DeathInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCC_OnCharacterDeathFinished, ACC_BaseCharacter*);

/**
 * 与玩法模式无关的服务器事件转发：角色只负责说"我死了"，模式规则（生存模式等）自己订阅。
 * 不保存状态、不复制；客户端上存在但不会收到广播。
 */
UCLASS()
class GAS_DEMO_API UCC_GameEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 便捷获取；World 为空或不是游戏世界时返回空。 */
	static UCC_GameEventSubsystem* Get(const UObject* WorldContext);

	/** 死亡开始（生命耗尽）。每次死亡只广播一次。 */
	FCC_OnCharacterDied OnCharacterDied;

	/** 死亡动作结束（死亡能力结束）。敌人随后会被销毁。 */
	FCC_OnCharacterDeathFinished OnCharacterDeathFinished;

	void BroadcastCharacterDied(const FCC_DeathInfo& Info)
	{
		OnCharacterDied.Broadcast(Info);
	}

	void BroadcastCharacterDeathFinished(ACC_BaseCharacter* Character)
	{
		OnCharacterDeathFinished.Broadcast(Character);
	}

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
