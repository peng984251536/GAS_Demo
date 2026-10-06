#pragma once

#include "CoreMinimal.h"
#include "Data/CC_TargetingTypes.h"
#include "UObject/Object.h"
#include "CC_CharacterRuntimeData.generated.h"

class UGameplayEffect;


/**
 * 每个角色独立的运行时数据，由 Character 创建默认子对象。
 * 只保存状态并提供读写/重置接口；属性监听、技能执行和身体表现仍由现有系统负责。
 * 不创建内容浏览器资产，不与其他角色共享此对象。
 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_CharacterRuntimeData : public UObject
{
	GENERATED_BODY()

public:
	/** 保留旧蓝图的初始 GE 配置。新配置资产指定 GE 后优先使用新配置。 */
	//UPROPERTY(EditDefaultsOnly, Category = "Crash|Legacy", meta = (DisplayName = "Initialize Attributes Effect (Legacy - Use Character Config)"))
	//TSubclassOf<UGameplayEffect> InitializeAttributesEffect;
	UPROPERTY(EditDefaultsOnly, Category = "Crash|Legacy", meta = (DisplayName = "base damage"))
	int DamageValue = 50;
	
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
#endif

	//--------------获取数据---------------//
	UFUNCTION(BlueprintPure, Category = "Crash|Runtime")
	bool IsAlive() const { return bAlive; }
	UFUNCTION(BlueprintPure, Category = "Crash|Runtime")
	bool IsHit() const { return bHit; }
	UFUNCTION(BlueprintPure, Category = "Crash|Runtime")
	FVector GetLastMoveInputDirection() const { return LastMoveInputDirection; }
	UFUNCTION(BlueprintPure, Category = "Crash|Runtime")
	FClosestActorWithTagResult GetClosestActor() const { return ClosestActor; }
	// C++ 修改接口。调用者负责网络权限；蓝图通过 Character 的入口修改。
	void SetAlive(bool bInAlive) { bAlive = bInAlive; }
	void SetHit(bool bInHit) { bHit = bInHit; }
	void SetLastMoveInputDirection(const FVector& InDirection);
	void SetClosestActor(const FClosestActorWithTagResult& InTarget);
	void ClearClosestActor() { ClosestActor.Reset(); }
	void ResetForRespawn();

private:
	UPROPERTY(Transient, Replicated, BlueprintReadOnly,
		Category = "Crash|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bAlive = false;
	UPROPERTY(Transient, Replicated, BlueprintReadOnly,
		Category = "Crash|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bHit = false;
	
	/** 本地输入/AI 写入；服务器的值同步给非拥有客户端，不覆盖拥有者的输入缓存。 */
	UPROPERTY(Transient, Replicated, BlueprintReadOnly,
		Category = "Crash|Runtime", meta = (AllowPrivateAccess = "true"))
	FVector LastMoveInputDirection = FVector::ForwardVector;
	/** 服务器 AI 的查询缓存；当前没有客户端消费需求，因此不复制。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Crash|Runtime",
		meta = (AllowPrivateAccess = "true"))
	FClosestActorWithTagResult ClosestActor;
	
};
