#include "AbilitySystem/Targeting/CC_TargetActor_GroundPlacement.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Targeting/CC_GroundPlacementIndicator.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"

namespace
{
/** 超出范围拉回后，向下重新找地面的上下搜索高度（厘米）。 */
constexpr float ClampedGroundSearchHeight = 500.f;
/** 胶囊离地的间隙，避免与地面本身重叠。 */
constexpr float CapsuleGroundClearance = 5.f;
}

ACC_TargetActor_GroundPlacement::ACC_TargetActor_GroundPlacement()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	// 只在本地生成；服务器等待客户端发来的 TargetData（见 UAbilityTask_WaitTargetData::ShouldSpawnTargetActor）。
	bReplicates = false;
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;

	GroundObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	CancelKeys.Add(EKeys::RightMouseButton);
	CancelKeys.Add(EKeys::Escape);
}

void ACC_TargetActor_GroundPlacement::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);

	const FGameplayAbilityActorInfo* Info = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	SourceActor = Info ? Info->AvatarActor.Get() : nullptr;
	if (!PrimaryPC && Info)
	{
		PrimaryPC = Info->PlayerController.Get();
	}
	if (!Info || !Info->IsLocallyControlled() || !PrimaryPC)
	{
		return;
	}

	if (IndicatorClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Indicator = GetWorld()->SpawnActor<ACC_GroundPlacementIndicator>(IndicatorClass, GetActorTransform(), Params);
		if (Indicator)
		{
			Indicator->SetRadius(IndicatorRadius > 0.f ? IndicatorRadius : CapsuleRadius);
			Indicator->SetActorHiddenInGame(true);
		}
	}
	SetCursorOverride(true);
	SetActorTickEnabled(true);
}

void ACC_TargetActor_GroundPlacement::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PrimaryPC || !PrimaryPC->IsLocalController())
	{
		return;
	}

	for (const FKey& Key : CancelKeys)
	{
		if (PrimaryPC->WasInputKeyJustPressed(Key))
		{
			CancelTargeting();
			return;
		}
	}

	FVector Location;
	bool bValid = false;
	const bool bHasGround = ComputePlacement(Location, bValid);
	bValidPlacement = bHasGround && bValid;
	if (bHasGround)
	{
		PlacementLocation = Location;
		SetActorLocation(Location);
	}
	if (Indicator)
	{
		Indicator->SetActorHiddenInGame(!bHasGround);
		if (bHasGround)
		{
			Indicator->UpdatePlacement(Location, bValidPlacement);
		}
	}
}

bool ACC_TargetActor_GroundPlacement::ComputePlacement(FVector& OutLocation, bool& bOutValid) const
{
	bOutValid = false;
	UWorld* World = GetWorld();
	FVector RayOrigin, RayDirection;
	if (!World || !PrimaryPC || !PrimaryPC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	const FCollisionObjectQueryParams ObjectParams(GroundObjectTypes);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CCGroundPlacement), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Indicator.Get());
	QueryParams.AddIgnoredActor(SourceActor.Get());

	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, RayOrigin, RayOrigin + RayDirection * TraceDistance, ObjectParams, QueryParams))
	{
		return false;
	}
	FVector Location = Hit.ImpactPoint;
	FVector Normal = Hit.ImpactNormal;

	// 超出范围：沿水平方向拉回到范围边缘，再向下重新找地面。
	if (MaxRange > 0.f && IsValid(SourceActor))
	{
		const FVector Origin = SourceActor->GetActorLocation();
		const FVector Delta = FVector(Location.X - Origin.X, Location.Y - Origin.Y, 0.f);
		if (Delta.SizeSquared() > FMath::Square(MaxRange))
		{
			const FVector Clamped = Origin + Delta.GetSafeNormal() * MaxRange;
			const FVector Start(Clamped.X, Clamped.Y, Location.Z + ClampedGroundSearchHeight);
			const FVector End(Clamped.X, Clamped.Y, Location.Z - ClampedGroundSearchHeight);
			FHitResult Down;
			if (!World->LineTraceSingleByObjectType(Down, Start, End, ObjectParams, QueryParams))
			{
				OutLocation = FVector(Clamped.X, Clamped.Y, Location.Z);
				return true; // 有显示位置，但不可放置（边缘外是悬崖 / 空洞）。
			}
			Location = Down.ImpactPoint;
			Normal = Down.ImpactNormal;
		}
	}

	OutLocation = Location;
	bOutValid = Normal.Z >= MinGroundNormalZ && IsLocationPlaceable(Location);
	return true;
}

bool ACC_TargetActor_GroundPlacement::IsLocationPlaceable(const FVector& GroundLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (CapsuleRadius > 0.f && CapsuleHalfHeight > 0.f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CCGroundPlacementRoom), false);
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(Indicator.Get());
		const FVector Center = GroundLocation + FVector(0.f, 0.f, CapsuleHalfHeight + CapsuleGroundClearance);
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
		if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, RoomCheckChannel, Capsule, Params))
		{
			return false;
		}
	}

	// 客户端通常没有导航数据（GetCurrent 返回空），此时跳过；服务器在生成前会再投影一次。
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		if (Nav->GetDefaultNavDataInstance())
		{
			FNavLocation NavLocation;
			if (!Nav->ProjectPointToNavigation(GroundLocation, NavLocation, NavProjectExtent))
			{
				return false;
			}
		}
	}
	return true;
}

bool ACC_TargetActor_GroundPlacement::IsConfirmTargetingAllowed()
{
	return bValidPlacement;
}

void ACC_TargetActor_GroundPlacement::ConfirmTargeting()
{
	if (!IsConfirmTargetingAllowed())
	{
		RebindLocalConfirm();
		return;
	}
	Super::ConfirmTargeting();
}

void ACC_TargetActor_GroundPlacement::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (IsValid(SourceActor))
	{
		TargetDataReadyDelegate.Broadcast(MakePlacementTargetData());
	}
}

void ACC_TargetActor_GroundPlacement::CancelTargeting()
{
	SetActorTickEnabled(false);
	Super::CancelTargeting();
}

void ACC_TargetActor_GroundPlacement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 确认、取消、能力被打断（死亡 / 撤回）最终都会销毁本 Actor，这里统一回收指示器和鼠标状态。
	if (IsValid(Indicator))
	{
		Indicator->Destroy();
	}
	Indicator = nullptr;
	SetCursorOverride(false);
	Super::EndPlay(EndPlayReason);
}

FGameplayAbilityTargetDataHandle ACC_TargetActor_GroundPlacement::MakePlacementTargetData() const
{
	FGameplayAbilityTargetingLocationInfo Target;
	Target.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Target.LiteralTransform = FTransform(PlacementLocation);

	FGameplayAbilityTargetData_LocationInfo* Data = new FGameplayAbilityTargetData_LocationInfo();
	Data->SourceLocation = StartLocation;
	Data->TargetLocation = Target;
	return FGameplayAbilityTargetDataHandle(Data);
}

void ACC_TargetActor_GroundPlacement::RebindLocalConfirm()
{
	const FGameplayAbilityActorInfo* Info = OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr;
	UAbilitySystemComponent* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
	if (ASC && Info->IsLocallyControlled())
	{
		ASC->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &ThisClass::ConfirmTargeting);
	}
}

void ACC_TargetActor_GroundPlacement::SetCursorOverride(bool bEnable)
{
	if (!bShowMouseCursorWhileTargeting || !PrimaryPC || !PrimaryPC->IsLocalController())
	{
		return;
	}
	if (bEnable && !bCursorOverridden)
	{
		bSavedShowMouseCursor = PrimaryPC->ShouldShowMouseCursor();
		PrimaryPC->SetShowMouseCursor(true);
		bCursorOverridden = true;
	}
	else if (!bEnable && bCursorOverridden)
	{
		PrimaryPC->SetShowMouseCursor(bSavedShowMouseCursor);
		bCursorOverridden = false;
	}
}
