#pragma once

#include "CoreMinimal.h"
#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/Survival/CC_SurvivalUITypes.h"
#include "CC_SurvivalResultController.generated.h"

class USaveGame;

/** 结算页展示模型：只保存快照。 */
UCLASS()
class GAS_DEMO_API UCC_SurvivalResultModel : public UCC_UIModel
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "UI|Survival")
	FCC_SurvivalResultSnapshot GetResult() const { return Result; }
private:
	void SetResult(const FCC_SurvivalResultSnapshot& Value) { Result = Value; NotifyChanged(); }
	UPROPERTY(Transient) FCC_SurvivalResultSnapshot Result;
	friend class UCC_SurvivalResultController;
};

/**
 * 结算页控制器：激活时从生存模式 GameState / 本地 PlayerState 读取结果，
 * 异步读取本地最佳纪录并在破纪录时写回（同一页面会话只处理一次）。
 * 按钮：再来一局 / 返回大厅（仅房主或单机可用，经 PlayerState 的 Server RPC 发给服务器）。
 * 页面蓝图：父类 CC_ActivatableWidget，Controller Class 选本类，在 On UI Model Changed 里读 GetResult。
 */
UCLASS()
class GAS_DEMO_API UCC_SurvivalResultController : public UCC_UIController
{
	GENERATED_BODY()
public:
	UCC_SurvivalResultController();

	UFUNCTION(BlueprintCallable, Category = "UI|Survival")
	bool RequestPlayAgain();

	UFUNCTION(BlueprintCallable, Category = "UI|Survival")
	bool RequestReturnToLobby();

protected:
	virtual void OnActivated() override;

private:
	FCC_SurvivalResultSnapshot BuildSnapshot() const;
	void HandleRecordLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* Loaded, FGuid Token);
	int32 GetLocalUserIndex() const;
};
