#pragma once

#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/Combat/CC_CombatTypes.h"
#include "AttributeSet.h"
#include "CC_CombatMainController.generated.h"

class UAbilitySystemComponent;
class ACC_BaseCharacter;
class APawn;
struct FOnAttributeChangeData;

/**
 * 战斗主界面展示模型：仅保存快照，不直接订阅 GAS，也不持有控件。
 * 以后战斗界面需要的数据（技能冷却、当前目标、Buff 等）都加在这里，由 CombatMainController 写入。
 */
UCLASS()
class GAS_DEMO_API UCC_CombatMainModel : public UCC_UIModel
{
	GENERATED_BODY()
public:
	/** 蓝图以一份快照读取玩家血蓝数据。 */
	UFUNCTION(BlueprintPure, Category="UI|Combat")
	FCC_PlayerVitals GetVitals() const { return Vitals; }
private:
	/** 控制器一次性写入四项属性，有变化时广播。 */
	void SetVitals(const FCC_PlayerVitals& Value);
	/** 本地展示副本，真实属性仍由 GAS 管理。 */
	UPROPERTY(Transient) FCC_PlayerVitals Vitals;
	friend class UCC_CombatMainController;
};

/** 战斗主界面控制器：监听 Pawn/ASC 生命周期，把 GAS 数据转换为展示快照。 */
UCLASS()
class GAS_DEMO_API UCC_CombatMainController : public UCC_UIController
{
	GENERATED_BODY()
public:
	/** 指定战斗主界面专用模型。 */
	UCC_CombatMainController();
protected:
	/** 监听角色切换，并立即绑定当前 Pawn。 */
	virtual void OnActivated() override;
	/** 解除玩家、角色和 ASC 订阅。 */
	virtual void OnDeactivated() override;
private:
	/** 解绑旧 Pawn，订阅新 Pawn 的 ASC 就绪事件。 */
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	/** ASC 晚于 Pawn 初始化时补做绑定。 */
	UFUNCTION() void HandleASCReady(UAbilitySystemComponent* NewASC, UAttributeSet* Attributes);
	/** 订阅四项属性并读取初值。 */
	void BindASC(UAbilitySystemComponent* NewASC);
	/** 按句柄移除本控制器的属性订阅。 */
	void UnbindASC();
	/** 清理旧角色及其 ASC。 */
	void UnbindPawn();
	/** 读取属性快照并提交给模型。 */
	void Refresh();
	/** 任意相关属性变化都刷新快照。 */
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	/** 当前角色弱引用，不延长角色生命。 */
	TWeakObjectPtr<ACC_BaseCharacter> BoundCharacter;
	/** 当前能力系统弱引用。 */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	/** 本控制器创建的四项属性委托句柄。 */
	TMap<FGameplayAttribute, FDelegateHandle> AttributeHandles;
};
