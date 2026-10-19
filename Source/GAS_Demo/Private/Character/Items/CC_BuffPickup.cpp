#include "Character/Items/CC_BuffPickup.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "GameplayEffect.h"
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
    if (Sphere.Succeeded())
    {
        Mesh->SetStaticMesh(Sphere.Object);
    }
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
    if (!HasAuthority())
    {
        PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }
    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::CheckInitialOverlaps);
}

void ACC_BuffPickup::CheckInitialOverlaps()
{
    TArray<AActor*> Overlapping;
    PickupSphere->GetOverlappingActors(Overlapping, ACC_BaseCharacter::StaticClass());
    for (AActor* Actor : Overlapping)
    {
        OnPickupOverlap(PickupSphere, Actor, nullptr, 0, false, FHitResult());
    }
}

/**
 * 只在服务器尝试施加通用 BuffEffect，不识别具体 Buff 类型。
 * 先占用拾取标志防止施加时的同步重入；失败会释放占用，成功才销毁道具。
 * @param Component 触发重叠的拾取组件。
 * @param OtherActor 进入拾取范围的角色。
 * @param OtherComponent 对方重叠组件。
 * @param BodyIndex 对方碰撞体索引。
 * @param bFromSweep 是否来自扫掠。
 * @param Hit 重叠携带的命中结果；当前拾取逻辑不使用。
 */
void ACC_BuffPickup::OnPickupOverlap(UPrimitiveComponent* Component, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep,
                                     const FHitResult& Hit)
{
    ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(OtherActor);
    if (!HasAuthority() || bConsumed || !Character || !Character->IsAlive() ||
        (bPlayersOnly && !Character->IsPlayerControlled()) || !BuffEffect)
    {
        return;
    }
    bConsumed = true;
    const FActiveGameplayEffectHandle Result = Character->GetBuffComponent()->ApplyBuff(BuffEffect, nullptr, BuffLevel);
    if (!Result.WasSuccessfullyApplied())
    {
        bConsumed = false;
        return;
    }
    Destroy();
}
