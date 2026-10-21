#pragma once

#include "UI/Framework/CC_UIController.h"
#include "UI/Character/CC_CharacterScreenTypes.h"
#include "AttributeSet.h"
#include "CC_CharacterScreenController.generated.h"

class UAbilitySystemComponent;
class ACC_BaseCharacter;
class UCC_BuffComponent;
class APawn;
struct FOnAttributeChangeData;

/**
 * 角色面板控制器：跟随本地玩家当前 Pawn，
 * 从 ASC 读属性、从 UCC_BuffComponent 的复制快照读 Buff，转换成展示数据写入模型。
 * 只读，不施加、不移除任何 Buff；不认识任何具体 Buff（CLAUDE.md 规则 1）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_CharacterScreenController : public UCC_UIController
{
	GENERATED_BODY()
public:
	/** 指定角色面板模型，并填入默认属性列表。 */
	UCC_CharacterScreenController();

protected:
	/** 监听 Pawn 切换并立即绑定当前 Pawn。 */
	virtual void OnActivated() override;
	/** 解除玩家、角色、ASC、BuffComponent 上的全部订阅。 */
	virtual void OnDeactivated() override;

	/** 左侧显示的属性（血蓝固定显示，不在此列表）。默认：攻击、防御、暴击率、暴击伤害、移速。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Character")
	TArray<FCC_CharacterStatConfig> DisplayedStats;

	/** 可选：带这个标签（资产标签或授予标签）的 Buff 标记为减益。留空则全部视为增益。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Character")
	FGameplayTag DebuffTag;

private:
	// ---- 数据源生命周期（与 CC_CombatMainController 相同的模式）----
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void HandleASCReady(UAbilitySystemComponent* NewASC, UAttributeSet* Attributes);
	void BindASC(UAbilitySystemComponent* NewASC);
	void UnbindASC();
	void BindBuffComponent(UCC_BuffComponent* NewBuffs);
	void UnbindBuffComponent();
	void UnbindPawn();

	// ---- 属性 ----
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshStats();

	// ---- Buff：BuffComponent 在服务器的增删改和客户端收到复制时都会广播 ----
	UFUNCTION() void HandleBuffsChanged();
	void RefreshBuffs();

	TWeakObjectPtr<ACC_BaseCharacter> BoundCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UCC_BuffComponent> BoundBuffs;
	/** 本控制器创建的属性委托句柄，按句柄精确移除。 */
	TMap<FGameplayAttribute, FDelegateHandle> AttributeHandles;
};
