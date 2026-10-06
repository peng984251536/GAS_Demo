// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/CC_AbilitySystemComponent.h"

#include "GameplayTags/CC_Tags.h"


// Sets default values for this component's properties
UCC_AbilitySystemComponent::UCC_AbilitySystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void UCC_AbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ActiveCount 已经减掉之后才会触发这个委托，
	// 因此这里可以安全尝试激活排队能力。
	// AbilityEndedCallbacks.AddUObject(
	// 	this,
	// 	&ThisClass::HandleAbilityEnded);
}

/**
 * 加载时，顺便激活自动能力
 * @param AbilitySpec 
 */
void UCC_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);


	// if(!HasAuthority())
	// 	return;

	HandleAutoActivatedAbility(AbilitySpec);
	
}

void UCC_AbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	// 拿到激活的能力，激活其中  自动激活的
	FScopedAbilityListLock ActiveScopeLock(*this);
	for(const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		HandleAutoActivatedAbility(AbilitySpec);
	}
	
	
}

void UCC_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	if(!IsValid(AbilitySpec.Ability))
		return;
	// 授予时，自动执行带有 ActivateOnGiven 的标签
	for(const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
	{
		if(Tag.MatchesTagExact(CCTags::CCAbilities::ActivateOnGiven))
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UCC_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if(!IsValid(GetAvatarActor()))
		return;
	if(!GetAvatarActor()->HasAuthority())
		return;

	if(FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
	{
		AbilitySpec->Level = Level;
	}
}

void UCC_AbilitySystemComponent::AddAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if(!IsValid(GetAvatarActor()))
		return;
	if(!GetAvatarActor()->HasAuthority())
		return;

	if(FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
	{
		AbilitySpec->Level += Level;
	}
}

#pragma region 操作相关

/**
 * 判断某个能力是否在运行
 * @param AbilityTag 
 * @return 
 */
bool UCC_AbilitySystemComponent::IsAbilityWithTagActive(
	const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !IsValid(Spec.Ability))
		{
			continue;
		}

		if (Spec.Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			return true;
		}
	}

	return false;
}

// /**
//  * 输入缓存
//  * @param AbilityTag 
//  * @param WaitForAbilityTag 
//  * @param LinkType 
//  */
// void UCC_AbilitySystemComponent::CacheInput(
// 	const FGameplayTag& AbilityTag,
// 	const FGameplayTag& WaitForAbilityTag,
// 	ECCAbilityLinkType LinkType)
// {
// 	if (!AbilityTag.IsValid() || !GetWorld())
// 	{
// 		return;
// 	}
//
// 	// if(GEngine)
// 	// {
// 	// 	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// 	// 	FString::Printf(TEXT("Cache Input")));
// 	// }
// 	
// 	BufferedInput.AbilityTag = AbilityTag;
// 	BufferedInput.WaitForAbilityTag = WaitForAbilityTag;
// 	BufferedInput.InputTime = GetWorld()->GetTimeSeconds();
// 	BufferedInput.LinkType = LinkType;
// }
//
// /**
//  * 使用队列管理输入
//  * @param WaitForAbilityTag 
//  * @param NewAbilityTag 
//  * @return 
//  */
// bool UCC_AbilitySystemComponent::HandleQueuedInput(
// 	const FGameplayTag& WaitForAbilityTag,
// 	const FGameplayTag& NewAbilityTag)
// {
// 	if (!NewAbilityTag.IsValid())
// 	{
// 		return false;
// 	}
//
// 	// 当前没有需要等待的能力，直接激活。
// 	if (!IsAbilityWithTagActive(WaitForAbilityTag))
// 	{
// 		return TryActivateAbilitiesByTag(
// 			NewAbilityTag.GetSingleTagContainer());
// 	}
//
// 	// 当前能力还在运行，保存为 Queue。
// 	CacheInput(
// 		NewAbilityTag,
// 		WaitForAbilityTag,
// 		ECCAbilityLinkType::Queue);
//
// 	return false;
// }
//
// /**
//  * 当前能力结束后，执行队列的能力
//  * @param EndedAbility 
//  */
// void UCC_AbilitySystemComponent::HandleAbilityEnded(
// 	UGameplayAbility* EndedAbility)
// {
// 	if (BufferedInput.LinkType != ECCAbilityLinkType::Queue)
// 	{
// 		return;
// 	}
//
// 	// 可能还有另一个带相同 Tag 的能力在执行。
// 	if (IsAbilityWithTagActive(
// 		BufferedInput.WaitForAbilityTag))
// 	{
// 		return;
// 	}
//
// 	if (!IsBufferedInputFresh(DefaultBufferTime))
// 	{
// 		BufferedInput.Reset();
// 		return;
// 	}
//
// 	const FGameplayTag QueuedAbilityTag =
// 		BufferedInput.AbilityTag;
//
// 	// 激活前清理，防止回调重入或重复执行。
// 	BufferedInput.Reset();
//
// 	TryActivateAbilitiesByTag(
// 		QueuedAbilityTag.GetSingleTagContainer());
// }
//
// /**
//  * 缓存是否过期
//  * @param MaxBufferTime 
//  * @return 
//  */
// bool UCC_AbilitySystemComponent::IsBufferedInputFresh(
// 	float MaxBufferTime) const
// {
// 	if (!BufferedInput.IsValid() || !GetWorld())
// 	{
// 		return false;
// 	}
//
// 	const float InputAge =
// 		GetWorld()->GetTimeSeconds() - BufferedInput.InputTime;
//
// 	return InputAge >= 0.0f
// 		&& InputAge <= MaxBufferTime;
// }
//
// /**
//  * 攻击连击管理
//  * @param AttackAbilityTag 
//  * @return 
//  */
// bool UCC_AbilitySystemComponent::HandleComboInput(
// 	const FGameplayTag& AttackAbilityTag)
// {
// 	if (!AttackAbilityTag.IsValid())
// 	{
// 		return false;
// 	}
//
// 	// 当前没有攻击能力，第一次按键直接激活。
// 	if (!IsAbilityWithTagActive(AttackAbilityTag))
// 	{
// 		return TryActivateAbilitiesByTag(
// 			AttackAbilityTag.GetSingleTagContainer());
// 	}
//
// 	// 攻击正在运行，再次按键缓存为连段输入。
// 	CacheInput(
// 		AttackAbilityTag,
// 		AttackAbilityTag,
// 		ECCAbilityLinkType::Combo);
//
// 	return false;
// }
//
// /**
//  * 消费连击
//  * @param AttackAbilityTag 
//  * @param MaxBufferTime 
//  * @return 
//  */
// bool UCC_AbilitySystemComponent::ConsumeComboInput(
// 	const FGameplayTag& AttackAbilityTag,
// 	float MaxBufferTime)
// {
// 	if (BufferedInput.LinkType != ECCAbilityLinkType::Combo)
// 	{
// 		if(GEngine)
// 		{
// 			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// 			FString::Printf(TEXT("1111111111")));
// 		}
// 		return false;
// 	}
//
// 	if (!BufferedInput.AbilityTag.MatchesTagExact(
// 		AttackAbilityTag))
// 	{
// 		if(GEngine)
// 		{
// 			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// 			FString::Printf(TEXT("222222222222")));
// 		}
// 		return false;
// 	}
//
// 	if (!IsBufferedInputFresh(MaxBufferTime))
// 	{
// 		if(GEngine)
// 		{
// 			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
// 			FString::Printf(TEXT("3333333333333")));
// 		}
// 		BufferedInput.Reset();
// 		return false;
// 	}
//
// 	BufferedInput.Reset();
// 	return true;
// }
//
// /**
//  * cancel 打断当前能力（用于闪避、受击等等）
//  * @param CancelAbilityTag 
//  * @param NewAbilityTag 
//  * @return 
//  */
// bool UCC_AbilitySystemComponent::CancelAndActivate(
// 	const FGameplayTag& CancelAbilityTag,
// 	const FGameplayTag& NewAbilityTag)
// {
// 	if (!CancelAbilityTag.IsValid() || !NewAbilityTag.IsValid())
// 	{
// 		return false;
// 	}
//
// 	// 必须先清理缓存。
// 	// 否则 CancelAbilities 触发能力结束回调时，
// 	// 可能错误执行之前排队的能力。
// 	BufferedInput.Reset();
//
// 	FGameplayTagContainer CancelTags;
// 	CancelTags.AddTag(CancelAbilityTag);
//
// 	CancelAbilities(&CancelTags);
//
// 	return TryActivateAbilitiesByTag(
// 		NewAbilityTag.GetSingleTagContainer());
// }
//
// /**
//  * 清理函数
//  */
// void UCC_AbilitySystemComponent::ClearBufferedInput()
// {
// 	BufferedInput.Reset();
// }
#pragma endregion  

#pragma region
FActiveGameplayEffectHandle UCC_AbilitySystemComponent::ApplyEffectSpecToSelf(FGameplayTag& gameplayTag,UClass* gameplayEffect)
{
	const FGameplayEffectSpecHandle ComboSpec = MakeOutgoingSpec(
	gameplayEffect, 1.f, MakeEffectContext());
	if (ComboSpec.IsValid())
	{
		ComboSpec.Data->DynamicGrantedTags.AddTag(gameplayTag);
		return  ApplyGameplayEffectSpecToSelf(*ComboSpec.Data.Get());
	}

	return {};
}
#pragma endregion 