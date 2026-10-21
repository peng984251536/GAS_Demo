#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Character/CC_CharacterScreenModel.h"
#include "CC_CharacterScreenWidget.generated.h"

class UPanelWidget;
class UCC_CharacterBuffEntryWidget;

/**
 * 角色面板页面基类：只读 CharacterScreenModel，不查找角色、不订阅 GAS 或 BuffComponent。
 * WBP 继承它后自由布局：
 *   - 左侧：实现 On Stats Changed（名字、血蓝、属性列表）。
 *   - 右侧：放一个名为 BuffList 的容器并设置 BuffEntryClass，由原生代码生成条目；
 *     或者只实现 On Buffs Changed 自己生成。
 * 推荐在 UI.Layer.GameMenu 打开。
 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_CharacterScreenWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()
public:
	/** 指定角色面板控制器；菜单输入，允许返回键关闭。 */
	UCC_CharacterScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** 某条 Buff 的剩余秒数；无限期返回 -1，已到期返回 0。按服务器时钟计算。 */
	UFUNCTION(BlueprintPure, Category="UI|Character")
	float GetBuffRemainingTime(const FCC_CharacterBuffEntry& Buff) const;

protected:
	/** 每次激活清空已读版本号，保证首次推送同时刷新属性和 Buff。 */
	virtual void NativeOnActivated() override;
	/** 按版本号只转发变化的部分。 */
	virtual void NativeOnModelChanged() override;

	/** 名字、血蓝或属性变化（含首次打开）。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Character")
	void OnStatsChanged(const FText& DisplayName, const FCC_PlayerVitals& Vitals, const TArray<FCC_CharacterStatEntry>& Stats);

	/** Buff 列表变化（含首次打开）；列表为空时也会触发，可用来显示“当前没有效果”。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Character")
	void OnBuffsChanged(const TArray<FCC_CharacterBuffEntry>& Buffs);

	/** 可选：Buff 条目容器（VerticalBox / ScrollBox 等），名称必须为 BuffList。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Character", meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> BuffList;

	/** 可选：配合 BuffList 自动生成的条目类。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Character")
	TSubclassOf<UCC_CharacterBuffEntryWidget> BuffEntryClass;

private:
	/** 按 InstanceId 复用已有条目，只新建新增的、移除消失的，保持顺序与列表一致。 */
	void SyncBuffList(const TArray<FCC_CharacterBuffEntry>& Buffs);

	UPROPERTY(Transient) TMap<int32, TObjectPtr<UCC_CharacterBuffEntryWidget>> EntriesById;
	int32 SeenStatsRevision = INDEX_NONE;
	int32 SeenBuffsRevision = INDEX_NONE;
};
