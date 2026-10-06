#include "AbilitySystem/Abilities/Logic/CC_ListenToHealth.h"
#include "GAS_Demo.h"

#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Engine/Engine.h"

UCC_ListenToHealth::UCC_ListenToHealth()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	bRetriggerInstancedAbility = false;

	DeadStateTag = CCTags::Status::Death;
	DeathAbilityTag = CCTags::CCAbilityTrigger::Death;
}

void UCC_ListenToHealth::PreActivate(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// 保留完整父类初始化：死亡回调需要 BaseCharacter 和 CombatComponent。
	// 监听本身无需 OwnedAction 或蒙太奇，父类允许不传动作数据。
	Super::PreActivate(Handle, ActorInfo, ActivationInfo,
		OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UCC_ListenToHealth::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	StopListening();
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || !ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ListeningASC = ASC;
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	// 与原蓝图一样等待后续属性变化，不在激活时额外触发死亡。
}

void UCC_ListenToHealth::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// if (ShowDebug && GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
	// 		FString::Printf(TEXT("%s: GA_Health NewValue: %g"),
	// 			*GetNameSafe(GetAvatarActorFromActorInfo()), Data.NewValue));
	// }

	
	UAbilitySystemComponent* ASC = ListeningASC.Get();
	if (!IsActive() || !IsValid(ASC))
	{
		return;
	}

	if (ShowDebug && GEngine)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("%s: GA_Health NewValue: %g"),
				*GetNameSafe(GetAvatarActorFromActorInfo()), Data.NewValue);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("%s: GA_Health NewValue: %g"),
				*GetNameSafe(GetAvatarActorFromActorInfo()), Data.NewValue));
	}

	// 客户端仍可监听复制的血量，但死亡能力只由服务器激活。
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()
		|| !(Data.NewValue < DeathHealthThreshold) || bActivatingDeathAbility
		|| !DeadStateTag.IsValid() || !DeathAbilityTag.IsValid()
		|| ASC->HasMatchingGameplayTag(DeadStateTag))
	{
		return;
	}

	// 死亡能力可能同步修改血量，防止属性委托重入。
	TGuardValue<bool> ActivationGuard(bActivatingDeathAbility, true);
	if (!IsValid(BaseCharacter))
	{
		return;
	}
	UActionComponent* Actions = BaseCharacter->GetUCombatActionComponent();
	if (!IsValid(Actions))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Cannot trigger death without an action component."), *GetNameSafe(BaseCharacter));
		return;
	}
	Actions->TryGameplayAbilityByTag(DeathAbilityTag, ASC, ASC);
}

void UCC_ListenToHealth::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	StopListening();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCC_ListenToHealth::BeginDestroy()
{
	StopListening();
	Super::BeginDestroy();
}

void UCC_ListenToHealth::StopListening()
{
	if (UAbilitySystemComponent* ASC = ListeningASC.Get(); ASC && HealthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	ListeningASC.Reset();
}
