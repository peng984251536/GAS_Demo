// Fill out your copyright notice in the Description page of Project Settings.


#include "Components\ActionComponent.h"
#include "GAS_Demo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CC_BaseCharacter.h"
#include "Character/Combat/CC_ArrowProjectile.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"


// namespace
// {
// 	FGameplayTag GetPlayActionEventTag()
// 	{
// 		return FGameplayTag::RequestGameplayTag(
// 			TEXT("Event.Combat.PlayActionMontage"));
// 	}
// }

UActionComponent::UActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UActionComponent::BeginPlay()
{
	Super::BeginPlay();
}



/**
 * 告诉组件：激活了某个动作
 * 获取这个攻击动作是第几段
 * @param Action 
 * @return 
 */
int32 UActionComponent::GetAttackIndex(UCombatActionData* Action)
{
	const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(ResolveASC()->GetAvatarActor());
	const UCC_CharacterConfig* config = Character->GetCharacterConfig();
	const UCombatActionSet* AttackActionSet = config->AttackActionSet; 
	
	++CurrentGeneration;
	//CurrentGeneration = CurrentGeneration%(AttackActionSet->GetComboLength()+1);
	CurrentAction = Action;
	bCanCombo = false;

	const int32 ComboMaxLength = IsValid(config->AttackActionSet) ? AttackActionSet->GetComboLength() : 0;
	const int32 index = AttackActionSet->FindActionIndexByCombo(Action);
	if (IsValid(Action) && index >= 0 && ComboMaxLength > 0)
	{
		NextComboIndex = (index + 1) % ComboMaxLength;
	}
	else
	{
		NextComboIndex = 0;
	}
	UE_LOG(LogGAS_Demo, Warning,
		TEXT("%s: Generation=%d, ComboLength=%d, ActionIndex=%d, Next=%d"),
		*GetOwner()->GetName(),
		CurrentGeneration,
		ComboMaxLength,
		index,
		NextComboIndex);
	return CurrentGeneration;
}

/**
 * 
 * @param ExpectedAction 
 * @param ExpectedGeneration 
 */
void UActionComponent::EndActionIfOwned(const UCombatActionData* ExpectedAction)
{
	// 蒙太奇结束回调可能在角色死亡/销毁后触发，此时 ASC/Config 已不可用。
	const UCC_AbilitySystemComponent* ASC = ResolveASC();
	const ACC_BaseCharacter* Character = IsValid(ASC)
		? Cast<ACC_BaseCharacter>(ASC->GetAvatarActor()) : nullptr;
	const UCC_CharacterConfig* Config = IsValid(Character) ? Character->GetCharacterConfig() : nullptr;
	const UCombatActionSet* AttackActionSet = IsValid(Config) ? Config->AttackActionSet : nullptr;

	if (CurrentAction != ExpectedAction
		&& (!IsValid(AttackActionSet) || AttackActionSet->GetComboLength() != 1))
	{
		return;
	}
	
	// if (CurrentAction != ExpectedAction || CurrentGeneration != ExpectedGeneration)
	// {
	// 	return;
	// }
	if (CurrentAction != ExpectedAction && AttackActionSet->GetComboLength() != 1)
	{
		return;
	}

	CurrentAction = nullptr;
	bCanCombo = false;
	NextComboIndex = 0;
}

#pragma region ComboWindow连击窗口管理

void UActionComponent::OpenComboWindowForMontage(
	const UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(CurrentAction)
		|| CurrentAction->Montage != SourceAnimation)
	{
		if(GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UActionComponent::OpenCombo nullptr"));
			GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
				FString::Printf(TEXT("UActionComponent::OpenCombo nullptr")));
		}
		return;
	}

	// if(GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
	// 		FString::Printf(TEXT("UActionComponent::OpenCombo")));
	// }
	bCanCombo = true;
}

void UActionComponent::CloseComboWindowForMontage(
	const UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(CurrentAction)
		|| CurrentAction->Montage != SourceAnimation)
	{
		return;
	}

	bCanCombo = false;
}
#pragma endregion 

#pragma region 执行

/**
 * 播放下一个动作
 * @return 
 */
bool UActionComponent::TryAttack(FGameplayTag GameplayTag)
{
	UCC_AbilitySystemComponent* ASC = ResolveASC();
	if (!IsValid(ASC))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] TryAttack failed: Owner=%s ASC missing Tag=%s"),
			*GetNameSafe(GetOwner()), *GameplayTag.ToString());
		return false;
	}
	const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(ASC->GetAvatarActor());
	if (!IsValid(Character))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] TryAttack failed: Owner=%s Avatar missing Tag=%s"),
			*GetNameSafe(GetOwner()), *GameplayTag.ToString());
		return false;
	}
	const UCC_CharacterConfig* config = Character->GetCharacterConfig();
	if (!IsValid(config))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] TryAttack failed: Character=%s Config missing"), *GetNameSafe(Character));
		return false;
	}
	const UCombatActionSet* AttackActionSet = config->AttackActionSet; 
	if (!IsValid(AttackActionSet))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] TryAttack failed: Character=%s AttackActionSet missing"), *GetNameSafe(Character));
		return false;
	}

	// 动作仍在播放，但不在连招窗口内：不允许抢播下一段。
	if (IsValid(CurrentAction) && !bCanCombo)
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Action] TryAttack deferred: Character=%s CurrentAction=%s ComboWindowClosed"),
			*GetNameSafe(Character), *GetNameSafe(CurrentAction));
		return false;
	}
	const int32 DesiredComboIndex = IsValid(CurrentAction) ? NextComboIndex : 0;
	UCombatActionData* NextAction =
		AttackActionSet->FindActionByComboIndex(DesiredComboIndex);
	// 数据不完整时安全回到第一段。
	if (!IsValid(NextAction))
	{
		NextAction = AttackActionSet->FindActionByComboIndex(0);
	}
	if (!IsValid(NextAction) || !IsValid(NextAction->Montage))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] TryAttack failed: Character=%s Index=%d Action=%s Montage=%s"),
			*GetNameSafe(Character), DesiredComboIndex, *GetNameSafe(NextAction),
			IsValid(NextAction) ? *GetNameSafe(NextAction->Montage) : TEXT("None"));
		return false;
	}
	FGameplayEventData EventData;
	
	if(GameplayTag == CCTags::CCAbilityTrigger::AttackAction)
	{
		// 弓兵仍走行为树的攻击意图和原动作集；只在实际执行动作时
		// 把事件送给独立的放箭能力，近战角色维持原 AttackAction 事件。
		const FGameplayTag ExecutionTag = GameplayTag;
		EventData.EventTag = ExecutionTag;
		EventData.Instigator = GetOwner();
		EventData.Target = GetOwner();
		EventData.OptionalObject = NextAction;
		const int32 TriggeredCount = ASC->HandleGameplayEvent(ExecutionTag, &EventData);
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Action] Attack event: Character=%s Tag=%s Action=%s Montage=%s Triggered=%d"),
			*GetNameSafe(Character), *ExecutionTag.ToString(), *GetNameSafe(NextAction),
			*GetNameSafe(NextAction->Montage), TriggeredCount);
		return TriggeredCount > 0;
	}
	else if(GameplayTag == CCTags::CCAbilityTrigger::BowShoot)
	{
		// 弓兵仍走行为树的攻击意图和原动作集；只在实际执行动作时
		// 把事件送给独立的放箭能力，近战角色维持原 AttackAction 事件。
		const FGameplayTag ExecutionTag = GameplayTag;
		EventData.EventTag = ExecutionTag;
		EventData.Instigator = GetOwner();
		EventData.Target = GetOwner();
		EventData.OptionalObject = NextAction;
		const int32 TriggeredCount = ASC->HandleGameplayEvent(ExecutionTag, &EventData);
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Action] Bow event: Character=%s Tag=%s Action=%s Montage=%s Triggered=%d"),
			*GetNameSafe(Character), *ExecutionTag.ToString(), *GetNameSafe(NextAction),
			*GetNameSafe(NextAction->Montage), TriggeredCount);
		return TriggeredCount > 0;
	}

	UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Action] Unsupported attack tag: Character=%s Tag=%s"),
		*GetNameSafe(Character), *GameplayTag.ToString());
	return false;
}

bool UActionComponent::TryDodge(FGameplayTag GameplayTag)
{
	UCC_AbilitySystemComponent* ASC = ResolveASC();
	const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(ResolveASC()->GetAvatarActor());
	const UCC_CharacterConfig* config = Character->GetCharacterConfig();
	const UCombatActionData* DodgeActionData = config->DodgeActionData; 
	
	if (!IsValid(ASC) || !IsValid(DodgeActionData))
	{
		return false;
	}

	FGameplayEventData EventData;
	EventData.EventTag = GameplayTag;
	EventData.Instigator = GetOwner();
	EventData.Target = GetOwner();
	EventData.OptionalObject = DodgeActionData;
	
	return ASC->HandleGameplayEvent(GameplayTag, &EventData) > 0;
}

bool UActionComponent::TryGameplayAbilityByTag(FGameplayTag GameplayTag,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsValid(SourceASC)||!IsValid(TargetASC))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("Send gameplay event: SourceASC=%s SourceAvatar=%s TargetASC=%s TargetAvatar=%s"),
        	*GetNameSafe(SourceASC),
			*GetNameSafe(IsValid(SourceASC) ? SourceASC->GetAvatarActor() : nullptr),
        	*GetNameSafe(TargetASC),
			*GetNameSafe(IsValid(TargetASC) ? TargetASC->GetAvatarActor() : nullptr));
		return false;
	}
	if (GameplayTag == CCTags::CCAbilityTrigger::BeHit)
	{
		const ACC_BaseCharacter* HitCharacter = Cast<ACC_BaseCharacter>(TargetASC->GetAvatarActor());
		if (!IsValid(HitCharacter) || !HitCharacter->IsAlive() || HitCharacter->IsActorBeingDestroyed()
			|| TargetASC->HasMatchingGameplayTag(CCTags::Status::Death))
		{
			return false;
		}
	}
	UCC_AbilitySystemComponent* OwnerASC = ResolveASC();
	const ACC_BaseCharacter* Character = IsValid(OwnerASC)
		? Cast<ACC_BaseCharacter>(OwnerASC->GetAvatarActor()) : nullptr;
	const UCC_CharacterConfig* config = IsValid(Character) ? Character->GetCharacterConfig() : nullptr;
	if ((GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::BeHit) ||
		GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::Death))
		&& !IsValid(config))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("Cannot send %s: missing character configuration."), *GameplayTag.ToString());
		return false;
	}
	
	else if(GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::BeHit))
	{
		const UCombatActionData* BeHitActionData = config->BeHitActionData; 
		if (!IsValid(BeHitActionData))
		{
			return false;
		}

		FGameplayEventData EventData;
		EventData.EventTag = GameplayTag;
		EventData.Instigator = SourceASC->GetAvatarActor();//发起人
		EventData.Target = TargetASC->GetAvatarActor();//接收人
		EventData.OptionalObject = BeHitActionData;
		EventData.TargetData = TargetData;

		return TargetASC->HandleGameplayEvent(GameplayTag, &EventData) > 0;
	}
	else if(GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::Death))
	{
		const UCombatActionData* DeathActionData = config->DeathActionData; 
		if (!IsValid(DeathActionData))
		{
			return false;
		}

		FGameplayEventData EventData;
		EventData.EventTag = GameplayTag;
		EventData.Instigator = SourceASC->GetAvatarActor();//发起人
		EventData.Target = TargetASC->GetAvatarActor();//接收人
		EventData.OptionalObject = DeathActionData;

		return TargetASC->HandleGameplayEvent(GameplayTag, &EventData) > 0;
	}
	// else if(GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::FindPlayerTarget)
	// 	|| GameplayTag == FGameplayTag(CCTags::CCAbilityTrigger::FindRangedTarget))
	else
	{
		FGameplayEventData EventData;
		EventData.EventTag = GameplayTag;
		EventData.Instigator = SourceASC->GetAvatarActor();//发起人
		EventData.Target = TargetASC->GetAvatarActor();//接收人
		EventData.OptionalObject = nullptr;

		bool successful = TargetASC->HandleGameplayEvent(GameplayTag, &EventData) > 0;
		if(!successful)
		{
			UE_LOG(
				LogGAS_Demo,
				Error,
				TEXT("[Action][AbilityByTag] tag: %s"),
				*GameplayTag.ToString());
		}
		return successful;
	}
}

#pragma endregion 



#pragma region

UCC_AbilitySystemComponent* UActionComponent::ResolveASC() const
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if(!IsValid(ASC))
	{
		if(GEngine)
		{
			UE_LOG(LogGAS_Demo, Log, TEXT("UAbilitySystemComponent is nullptr"));
			GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
				FString::Printf(TEXT("UAbilitySystemComponent is nullptr")));
		}
		return nullptr;
	}
	else if(UCC_AbilitySystemComponent* UCC_ASC = Cast<UCC_AbilitySystemComponent>(ASC))
	{
		return UCC_ASC;
	}	

	if(GEngine)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("UAbilitySystemComponent is nullptr"));
		GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::Red,
			FString::Printf(TEXT("UAbilitySystemComponent is nullptr")));
	}
	return nullptr;
}

#pragma endregion 

