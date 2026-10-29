// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/CC_BlueprintLibrary.h"
#include "GAS_Demo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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


FLinearColor UCC_BlueprintLibrary::GetColorFromString(const FString& Text)
{
	const uint32 Hash = FCrc::StrCrc32(*Text);
	const float Hue = static_cast<float>(Hash % 360);

	// HSV：色相、饱和度、亮度、透明度
	return FLinearColor(Hue, 0.65f, 0.95f, 1.f).HSVToLinearRGB();
}
