#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "CC_WorldIndicatorSubsystem.generated.h"

class APlayerController;
class UCC_WorldIndicator;

DECLARE_MULTICAST_DELEGATE_OneParam(FCC_WorldIndicatorEvent, UCC_WorldIndicator*);

/**
 * 每个本地玩家一份的世界指示器列表。数据来源（血条、伤害数字、交互提示……）在这里增删指示器，
 * UCC_WorldIndicatorLayer 订阅增删事件并负责显示。只在增删时广播，不按帧广播。
 * 分屏时每个玩家各自一份；专用服务器没有本地玩家，不会创建。
 */
UCLASS()
class GAS_DEMO_API UCC_WorldIndicatorSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	/** 取本地玩家的子系统；不是本地玩家时返回 nullptr。 */
	UFUNCTION(BlueprintPure, Category="UI|World Indicator")
	static UCC_WorldIndicatorSubsystem* GetForPlayer(const APlayerController* PlayerController);

	/** 添加指示器；重复添加同一个对象会被忽略。 */
	UFUNCTION(BlueprintCallable, Category="UI|World Indicator")
	void AddIndicator(UCC_WorldIndicator* Indicator);
	/** 移除指示器；不存在时什么也不做。 */
	UFUNCTION(BlueprintCallable, Category="UI|World Indicator")
	void RemoveIndicator(UCC_WorldIndicator* Indicator);
	/** 移除全部指示器。 */
	UFUNCTION(BlueprintCallable, Category="UI|World Indicator")
	void ClearIndicators();

	const TArray<TObjectPtr<UCC_WorldIndicator>>& GetIndicators() const { return Indicators; }
	FCC_WorldIndicatorEvent OnIndicatorAdded;
	FCC_WorldIndicatorEvent OnIndicatorRemoved;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** 切图时世界被清理，旧世界的指示器全部移除（本地玩家会跨地图保留）。 */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCC_WorldIndicator>> Indicators;
	FDelegateHandle WorldCleanupHandle;
};
