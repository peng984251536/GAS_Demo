// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/CC_BlueprintLibrary.h"
#include "GAS_Demo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CC_BaseCharacter.h"
#include "Character/CC_EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Damage/CC_LyraStyleDamage.h"
#include "Damage/GASDamageExecutionBase.h"
#include "Kismet/GameplayStatics.h"

ACC_EnemyCharacter* UCC_BlueprintLibrary::SpawnEnemy(const UObject* WorldContextObject,
	TSubclassOf<ACC_EnemyCharacter> EnemyClass, const FTransform& SpawnTransform)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !IsValid(EnemyClass.Get()))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	ACC_EnemyCharacter* Enemy = World->SpawnActor<ACC_EnemyCharacter>(
		EnemyClass, SpawnTransform, SpawnParameters);
	if (IsValid(Enemy) && !Enemy->GetController())
	{
		Enemy->SpawnDefaultController();
	}
	// 属性和技能沿用怪物 BeginPlay 初始化，不在这里重复授予。
	return IsValid(Enemy) ? Enemy : nullptr;
}


/**
 * 获取向量
 * @param TargetForward 
 * @param ToInstigator 
 * @return 
 */
EHitDirection UCC_BlueprintLibrary::GetHitDirection(const FVector& TargetForward, const FVector& ToInstigator)
{
	// 点乘（攻击方向、怪物方向），得到是否
	const float Dot = FVector::DotProduct(TargetForward,ToInstigator);
	if(Dot<=-0.5f)
	{
		return EHitDirection::Back;
	}
	else if(Dot>-0.5f && Dot <0.5f)
	{
		const FVector Cross = FVector::CrossProduct(TargetForward,ToInstigator);
		if(Cross.Z<0)
		{
			return EHitDirection::Left;
		}
		else
		{
			return EHitDirection::Right;
		}
	}
	else
	{
		return EHitDirection::Forward0;
	}
}

/**
 * 获取向量
 * @param TargetForward 
 * @param ToInstigator 
 * @return 
 */
FString UCC_BlueprintLibrary::GetHitDirectionName(const EHitDirection Direction)
{
	// 点乘（攻击方向、怪物方向），得到是否
	if(Direction == EHitDirection::Forward0)
	{
		return "Forward";
	}
	else if(Direction == EHitDirection::Back)
	{
		return "Back";
	}
	else if(Direction == EHitDirection::Back)
	{
		return "Back";
	}
	else if(Direction == EHitDirection::Right)
	{
		return "Right";
	}

	return "Forward";
}

/**
 * 这个函数是使用便利所有实体的方式找到最近的目标
 * @param WorldContextObject 
 * @param Origin 
 * @param Tag 
 * @return 
 */
FClosestActorWithTagResult UCC_BlueprintLibrary::FindClosestActorWithTag(
	const UObject* WorldContextObject,
	const FVector& Origin, const FName& Tag)
{
	TArray<AActor*> ActorsWithTag;

	// 获取某一种类型的所以实体
	UGameplayStatics::GetAllActorsWithTag(
		WorldContextObject,
		Tag,
		ActorsWithTag);

	float ClosestDistance = TNumericLimits<float>::Max();

	AActor* ClosestActor = nullptr;

	for (AActor* Actor : ActorsWithTag)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		ACC_BaseCharacter* BaseCharacter = Cast<ACC_BaseCharacter>(Actor);

		if (!IsValid(BaseCharacter) ||
			!BaseCharacter->IsAlive())
		{
			continue;
		}

		const float Distance = FVector::Dist(
			Origin,
			Actor->GetActorLocation()
		);

		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestActor = Actor;
		}
	}

	FClosestActorWithTagResult Result;
	Result.Actor = ClosestActor;
	Result.LastKnownLocation = ClosestActor->GetActorLocation();

	return Result;
}

void UCC_BlueprintLibrary::ApplyGameplayEffectSpecByTag(
    UAbilitySystemComponent* SourceASC,
    UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> GAEffect,
	//const FGameplayEffectSpecHandle& SpecHandle,
	const FGameplayTag& SetByCallTag,
	int Damage)
{
	if (!IsValid(SourceASC) ||
		!IsValid(TargetASC) ||
		!GAEffect ||
		!SetByCallTag.IsValid())
	{
		UE_LOG(
			LogGAS_Demo,
			Warning,
			TEXT("ApplyGameplayEffectSpecByTag: Invalid input. SourceASC=%s TargetASC=%s Effect=%s Tag=%s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetASC),
			*GetNameSafe(GAEffect.Get()),
			*SetByCallTag.ToString());

		return;
	}
	
	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();

	FGameplayEffectSpecHandle SpecHandle =
		SourceASC->MakeOutgoingSpec(
			GAEffect,
			1.f,
			ContextHandle
			);

	//ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		(SpecHandle),
		SetByCallTag,
		-Damage
	);
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

/**
 * 伤害计算
 * @param SourceASC 
 * @param TargetASC 
 */
void UCC_BlueprintLibrary::ApplyHeavyAttackDamage(UAbilitySystemComponent* SourceASC,
                                                  UAbilitySystemComponent* TargetASC)
{
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	// 实际伤害由服务器结算。
	if (!SourceASC->IsOwnerActorAuthoritative()
		|| !TargetASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 1. 创建上下文，记录这次效果的来源。
	FGameplayEffectContextHandle Context =
		SourceASC->MakeEffectContext();

	// 如果有本次命中的 FHitResult，可在创建 Spec 前添加：
	// Context.AddHitResult(HitResult);

	// 2. 创建新的伤害 GE Spec。
	// 此时会按默认配置快照攻击者的 AttackPower。
	FGameplayEffectSpecHandle Spec =
		SourceASC->MakeOutgoingSpec(
			UCC_GE_LyraStyleDamage::StaticClass(),
			1.0f,   // GE 等级，不是伤害倍率
			Context);

	if (!Spec.IsValid())
	{
		return;
	}

	// 3. 填写这一次攻击的参数。
	Spec.Data->SetSetByCallerMagnitude(
		StandaloneDamageTags::SkillScale, 2.0f);

	Spec.Data->SetSetByCallerMagnitude(
		StandaloneDamageTags::FlatDamage, 20.0f);

	// 这两项默认就是 1，不填写也可以。
	Spec.Data->SetSetByCallerMagnitude(
		StandaloneDamageTags::DistanceMultiplier, 1.0f);

	Spec.Data->SetSetByCallerMagnitude(
		StandaloneDamageTags::HitMultiplier, 1.0f);

	// 4. 应用到受击者。
	// GAS 自动执行伤害计算，并输出 Health 扣减。
	SourceASC->ApplyGameplayEffectSpecToTarget(
		*Spec.Data.Get(),
		TargetASC);
}








