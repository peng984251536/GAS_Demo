// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "CC_BaseCharacter.h"


#include "CC_PlayerCharacter.generated.h"

class UCC_PlayerCrowdAgentComponent;


UCLASS()
class GAS_DEMO_API ACC_PlayerCharacter : public ACC_BaseCharacter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	float MyTargetArmLength = 500.f;

	// Sets default values for this character's properties
	ACC_PlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual  UActionComponent* GetUCombatActionComponent() const override;
	virtual void PossessedBy(AController* PlayerController) override;
	virtual void OnRep_PlayerState() override;
	bool TryActivateAbilities(TSubclassOf<UGameplayAbility> AbilityClass) const;
	virtual UCC_AttributeSet* GetAttributeSet() const override;
	/** 默认属于 CCTags.Team.Player；CharacterConfig.Team 有值时以配置为准。 */
	virtual FGameplayTag GetDefaultTeam() const override;

	/**
	 * 原地复用 Pawn 复活（服务器）。不走 PossessedBy → HandleRespawn，因为那条路径会清空全部 Buff。
	 * 依次：标记存活 → 取消死亡能力 → 传送 → 恢复移动/碰撞 → 施加 ReviveEffect（为空或施加后仍无血时回满血）
	 * → 恢复死亡时挂起的 Buff（UCC_BuffComponent::ResumeAfterRevive）。
	 * @param Location 复活落点（胶囊中心，调用方负责找可用位置）。
	 * @param ReviveEffect 复活时施加的 GE，例如回满血 + 短暂 Status.DamageImmunity；可为空。
	 * @return 已死亡且成功复活时返回 true。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crash|Respawn")
	bool Revive(const FVector& Location, TSubclassOf<UGameplayEffect> ReviveEffect);
	
	//蓝图事件
	// UFUNCTION(BlueprintImplementableEvent, Category="Crash|Input")
	// void OnAttackInput();

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;
	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr <USpringArmComponent> CameraBoom;
	/** 把玩家注册为 Detour Crowd 的障碍 Agent，敌人会提前绕开玩家（仅服务器）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCC_PlayerCrowdAgentComponent> CrowdAgent;

protected:
	/** 默认从角色蓝图读取；子类可覆盖配置来源。返回只读配置，不存放角色实例状态。 */
	virtual const UCC_CharacterConfig* GetCharacterConfig() const override;
	
};
