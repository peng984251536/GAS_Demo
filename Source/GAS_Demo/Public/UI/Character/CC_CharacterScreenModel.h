#pragma once

#include "UI/Framework/CC_UIModel.h"
#include "UI/Combat/CC_CombatTypes.h"
#include "UI/Character/CC_CharacterScreenTypes.h"
#include "CC_CharacterScreenModel.generated.h"

/**
 * 角色面板展示模型：只保存快照，不订阅 GAS 或 BuffComponent，也不持有控件。
 * 属性（含血蓝）和 Buff 各有一个版本号，界面据此只刷新变化的那一半，
 * 受伤时不会重建 Buff 列表，Buff 变化时也不会重刷属性。
 */
UCLASS()
class GAS_DEMO_API UCC_CharacterScreenModel : public UCC_UIModel
{
	GENERATED_BODY()
public:
	/** 角色显示名，取自 PlayerState。 */
	UFUNCTION(BlueprintPure, Category="UI|Character")
	FText GetDisplayName() const { return DisplayName; }

	/** 生命和法力，与战斗主界面共用同一结构。 */
	UFUNCTION(BlueprintPure, Category="UI|Character")
	FCC_PlayerVitals GetVitals() const { return Vitals; }

	/** 其余属性，顺序即控制器 DisplayedStats 的配置顺序。 */
	UFUNCTION(BlueprintPure, Category="UI|Character")
	const TArray<FCC_CharacterStatEntry>& GetStats() const { return Stats; }

	/** 当前 Buff，已过滤 bHiddenInUI，按施加先后排序。 */
	UFUNCTION(BlueprintPure, Category="UI|Character")
	const TArray<FCC_CharacterBuffEntry>& GetBuffs() const { return Buffs; }

	/** 名字、血蓝、属性任一变化加一。 */
	int32 GetStatsRevision() const { return StatsRevision; }
	/** Buff 列表变化加一。 */
	int32 GetBuffsRevision() const { return BuffsRevision; }

private:
	/** 三项一起提交，值都没变时不广播。 */
	void SetStats(const FText& InDisplayName, const FCC_PlayerVitals& InVitals, TArray<FCC_CharacterStatEntry>&& InStats);
	/** 列表没变时不广播。 */
	void SetBuffs(TArray<FCC_CharacterBuffEntry>&& InBuffs);

	UPROPERTY(Transient) FText DisplayName;
	UPROPERTY(Transient) FCC_PlayerVitals Vitals;
	UPROPERTY(Transient) TArray<FCC_CharacterStatEntry> Stats;
	UPROPERTY(Transient) TArray<FCC_CharacterBuffEntry> Buffs;
	int32 StatsRevision = 0;
	int32 BuffsRevision = 0;

	/** 写入口只对角色面板控制器开放。 */
	friend class UCC_CharacterScreenController;
};
