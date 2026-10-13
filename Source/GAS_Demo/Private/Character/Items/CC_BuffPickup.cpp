#include "Character/Items/CC_BuffPickup.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Effects/Buff/CC_GE_Buff.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACC_BuffPickup::ACC_BuffPickup()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    SetRootComponent(PickupSphere);
    PickupSphere->InitSphereRadius(PickupRadius);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PickupSphere->SetGenerateOverlapEvents(true);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(PickupSphere);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeScale3D(FVector(0.35f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
    PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnPickupOverlap);
}

void ACC_BuffPickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    PickupSphere->SetSphereRadius(FMath::Max(1.f, PickupRadius));
}

void ACC_BuffPickup::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) { PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); return; }
    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::CheckInitialOverlaps);
}

void ACC_BuffPickup::CheckInitialOverlaps()
{
    TArray<AActor*> Overlapping;
    PickupSphere->GetOverlappingActors(Overlapping, ACC_BaseCharacter::StaticClass());
    for (auto* Actor : Overlapping) OnPickupOverlap(PickupSphere, Actor, nullptr, 0, false, FHitResult());
}

void ACC_BuffPickup::OnPickupOverlap(UPrimitiveComponent* Component, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
    auto* Character = Cast<ACC_BaseCharacter>(OtherActor);
    if (!HasAuthority() || bConsumed || !Character || !Character->IsAlive()
        || (bPlayersOnly && !Character->IsPlayerControlled()) || !BuffEffect) return;
    bConsumed = true;
    const auto Result = Character->GetBuffComponent()->ApplyBuff(BuffEffect, nullptr, BuffLevel);
    if (!Result.WasSuccessfullyApplied()) { bConsumed = false; return; }
    Destroy();
}
