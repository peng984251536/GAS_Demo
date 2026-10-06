#include "Character/Spawner/CC_EnemySpawner.h"

#include "Character/CC_EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS_Demo.h"

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
	if (bSpawnOnBeginPlay)
	{
		SpawnEnemies();
	}
}

int32 ACC_EnemySpawner::SpawnEnemies()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || GetNetMode() == NM_Client || !HasAuthority() || bIsSpawning || SpawnCount <= 0)
	{
		return 0;
	}
	if (!EnemyClass || EnemyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Select a concrete EnemyClass before spawning."), *GetName());
		return 0;
	}

	TGuardValue<bool> SpawningGuard(bIsSpawning, true);
	const ACC_EnemyCharacter* Defaults = EnemyClass->GetDefaultObject<ACC_EnemyCharacter>();
	const float CapsuleHalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float WalkableFloorZ = Defaults->GetCharacterMovement()->GetWalkableFloorZ();
	const FVector Origin = GetActorLocation();
	const float Radius = FMath::Max(0.0f, SpawnRadius);
	const float SearchDistance = FMath::Max(1.0f, GroundSearchDistance);
	const int32 Attempts = FMath::Clamp(MaxAttemptsPerEnemy, 1, 100);
	// Snapshot settings: spawned actors can execute Blueprint code during FinishSpawning.
	const int32 RequestedCount = SpawnCount;
	const TSubclassOf<ACC_EnemyCharacter> ClassToSpawn = EnemyClass;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGround), false, this);
	const FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < RequestedCount && IsValid(this); ++Index)
	{
		for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
		{
			const float Angle = FMath::FRand() * 2.0f * PI;
			const float Distance = FMath::Sqrt(FMath::FRand()) * Radius;
			const FVector Candidate = Origin + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
			FHitResult GroundHit;
			if (!World->LineTraceSingleByObjectType(GroundHit,
				Candidate + FVector(0, 0, SearchDistance), Candidate - FVector(0, 0, SearchDistance),
				GroundObjects, QueryParams) || GroundHit.ImpactNormal.Z < WalkableFloorZ)
			{
				continue;
			}

			const FVector Location = GroundHit.ImpactPoint + FVector(0, 0, CapsuleHalfHeight + 2.0f);
			const FTransform SpawnTransform(FRotator(0, GetActorRotation().Yaw, 0), Location);
			ACC_EnemyCharacter* Enemy = World->SpawnActorDeferred<ACC_EnemyCharacter>(
				ClassToSpawn, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
			if (!Enemy)
			{
				continue;
			}
			// Also supports the original Boris blueprint, which only auto-possesses placed actors.
			Enemy->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
			Enemy->FinishSpawning(SpawnTransform);
			if (IsValid(Enemy))
			{
				++SpawnedCount;
				break;
			}
		}
	}
	if (SpawnedCount < RequestedCount)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("%s: Spawned %d/%d enemies. Check ground collision and available space (radius %.0f)."),
			*GetName(), SpawnedCount, RequestedCount, Radius);
	}
	return SpawnedCount;
}
