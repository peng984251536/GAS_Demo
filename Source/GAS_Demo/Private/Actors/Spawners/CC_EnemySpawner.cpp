#include "Actors/Spawners/CC_EnemySpawner.h"

#include "Actors/Character/CC_EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS_Demo.h"
#include "NavigationSystem.h"

ACC_EnemySpawner::ACC_EnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SpawnArea = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnArea"));
	SetRootComponent(SpawnArea);
	SpawnArea->InitSphereRadius(SpawnRadius);
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetGenerateOverlapEvents(false);
	SpawnArea->SetCanEverAffectNavigation(false);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->SetAbsolute(false, false, true);
	SpawnArea->ShapeColor = FColor::Red;
}

void ACC_EnemySpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SpawnArea->SetSphereRadius(FMath::Max(0.0f, SpawnRadius));
}

void ACC_EnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay && !bManagedByWaveDirector)
	{
		SpawnEnemies();
	}
}

bool ACC_EnemySpawner::CanSpawnHere() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld() && GetNetMode() != NM_Client && HasAuthority();
}

bool ACC_EnemySpawner::FindSpawnLocation(TSubclassOf<ACC_EnemyCharacter> Class, bool bProjectToNavigation,
	FRandomStream& Random, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World || !Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	const ACC_EnemyCharacter* Defaults = Class->GetDefaultObject<ACC_EnemyCharacter>();
	const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float WalkableFloorZ = Defaults->GetCharacterMovement()->GetWalkableFloorZ();
	const FVector Origin = GetActorLocation();
	const float Radius = FMath::Max(0.0f, SpawnRadius);
	const float SearchDistance = FMath::Max(1.0f, GroundSearchDistance);
	const int32 Attempts = FMath::Clamp(MaxAttemptsPerEnemy, 1, 100);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGround), false, this);
	const FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	const UNavigationSystemV1* NavSys = bProjectToNavigation ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;

	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const float Angle = Random.FRand() * 2.0f * PI;
		const float Distance = FMath::Sqrt(Random.FRand()) * Radius;
		const FVector Candidate = Origin + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
		FHitResult GroundHit;
		if (!World->LineTraceSingleByObjectType(GroundHit,
			Candidate + FVector(0, 0, SearchDistance), Candidate - FVector(0, 0, SearchDistance),
			GroundObjects, QueryParams) || GroundHit.ImpactNormal.Z < WalkableFloorZ)
		{
			continue;
		}

		FVector Ground = GroundHit.ImpactPoint;
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (!NavSys->ProjectPointToNavigation(Ground, NavLocation, FVector(CapsuleRadius * 2.f, CapsuleRadius * 2.f, CapsuleHalfHeight)))
			{
				continue;
			}
			Ground = NavLocation.Location;
		}

		const FVector Location = Ground + FVector(0, 0, CapsuleHalfHeight + 2.0f);
		if (World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, CapsuleShape, QueryParams))
		{
			continue;
		}
		OutLocation = Location;
		return true;
	}
	return false;
}

ACC_EnemyCharacter* ACC_EnemySpawner::SpawnEnemyAt(TSubclassOf<ACC_EnemyCharacter> Class, const FVector& Location,
	const FCC_EnemySpawnParams& Params)
{
	UWorld* World = GetWorld();
	if (!CanSpawnHere() || !Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	const FTransform SpawnTransform(FRotator(0, GetActorRotation().Yaw, 0), Location);
	ACC_EnemyCharacter* Enemy = World->SpawnActorDeferred<ACC_EnemyCharacter>(
		Class, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Enemy)
	{
		return nullptr;
	}
	if (Params.bIsSet)
	{
		Enemy->SetSpawnParams(Params);
	}
	// Also supports the original Boris blueprint, which only auto-possesses placed actors.
	Enemy->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Enemy->FinishSpawning(SpawnTransform);
	return IsValid(Enemy) ? Enemy : nullptr;
}

int32 ACC_EnemySpawner::SpawnEnemies()
{
	if (!CanSpawnHere() || bIsSpawning || SpawnCount <= 0)
	{
		return 0;
	}
	if (!EnemyClass || EnemyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Select a concrete EnemyClass before spawning."), *GetName());
		return 0;
	}

	TGuardValue<bool> SpawningGuard(bIsSpawning, true);
	// Snapshot settings: spawned actors can execute Blueprint code during FinishSpawning.
	const int32 RequestedCount = SpawnCount;
	const TSubclassOf<ACC_EnemyCharacter> ClassToSpawn = EnemyClass;
	FRandomStream Random(FMath::Rand());
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < RequestedCount && IsValid(this); ++Index)
	{
		FVector Location;
		// 原有行为：不要求导航网格。
		if (FindSpawnLocation(ClassToSpawn, false, Random, Location) && SpawnEnemyAt(ClassToSpawn, Location, FCC_EnemySpawnParams()))
		{
			++SpawnedCount;
		}
	}
	if (SpawnedCount < RequestedCount)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Spawned %d/%d enemies. Check ground collision and available space (radius %.0f)."),
			*GetName(), SpawnedCount, RequestedCount, FMath::Max(0.0f, SpawnRadius));
	}
	return SpawnedCount;
}
