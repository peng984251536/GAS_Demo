#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Debug/CC_DebugCVars.h"
#include "Actors/Projectiles/CC_ProjectileInteractions.h"
#include "Actors/Character/CC_BaseCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/ActionComponent.h"
#include "GameplayTags/CC_Tags.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Damage/CC_DamageStatics.h"
#include "Utils/Team/CC_TeamStatics.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "Damage/CC_DamageTypes.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "GAS_Demo.h"
#include "UObject/ConstructorHelpers.h"

ACC_ArrowProjectile::ACC_ArrowProjectile()
{
    // 仅用于调试绘制；BeginPlay 会按开关决定是否真正启用 Tick。
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(true);
    InitialLifeSpan = 5.0f;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(8.0f);
    // 只做查询（ProjectileMovement 的扫掠、Hit/Overlap 回调都属于查询），不参与物理，避免推挤角色。
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // 角色不能把箭当作地面踩上去；否则箭被销毁时会把它的速度传给角色，表现为被打飞。
    Collision->CanCharacterStepUpOn = ECB_No;
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    // Pawn 改为重叠：在 OnArrowOverlap 中按 IsHostile 判断，友方穿过、敌方命中。
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetGenerateOverlapEvents(true);
    Collision->SetNotifyRigidBodyCollision(true);
    RootComponent = Collision;

    ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
    ArrowMesh->SetupAttachment(Collision);
    ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrowMesh->CanCharacterStepUpOn = ECB_No;
    // 原生类给出可见的默认箭模型；蓝图子类仍可以替换网格及朝向。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
        TEXT("/Game/ModularRPGHeroesPolyart/Meshes/Weapons/Arrow01SM.Arrow01SM"));
    if (DefaultMesh.Succeeded())
    {
        ArrowMesh->SetStaticMesh(DefaultMesh.Object);
    }

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->UpdatedComponent = Collision;
    Movement->InitialSpeed = 1800.0f;
    Movement->MaxSpeed = 1800.0f;
    Movement->ProjectileGravityScale = 0.0f;

    //朝着速度方向旋转
    Movement->bRotationFollowsVelocity = true;
}

bool ACC_ArrowProjectile::InitializeArrow(float InSpeed, const FCC_ArrowPayload& InPayload)
{
    const float Speed = FMath::Max(1.0f, InSpeed);
    Movement->InitialSpeed = Speed;
    Movement->MaxSpeed = Speed;
    //Movement->Velocity = GetActorForwardVector() * Speed;
    Payload = InPayload;
    FCC_DamageRequest Request;
    Request.SkillScale = Payload.SkillScale;
    Request.FlatDamage = Payload.FlatDamage;
    Request.DamageMultiplier = Payload.DamageMultiplier;
    Request.SourceTags = Payload.SourceTags;
    DamageSnapshot = UCC_DamageStatics::PrepareDamage(
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetInstigator()), Request, this);
    UE_LOG(LogCCBuff, Verbose, TEXT("[Arrow][Snapshot] Arrow=%s Speed=%.1f Skill=%.3f Flat=%.1f Tags=%s Valid=%d"),
           *GetNameSafe(this), Speed, Request.SkillScale, Request.FlatDamage, *Request.SourceTags.ToStringSimple(),
           DamageSnapshot.IsValid());
    return DamageSnapshot.IsValid();
}

void ACC_ArrowProjectile::IgnoreVolleyArrow(ACC_ArrowProjectile* Other)
{
    if (IsValid(Other) && Other != this)
    {
        Collision->IgnoreActorWhenMoving(Other, true);
    }
}

bool ACC_ArrowProjectile::Deflect(APawn* NewInstigator)
{
    const auto* Character = Cast<ACC_BaseCharacter>(NewInstigator);
    auto* NewASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewInstigator);
    APawn* PreviousShooter = GetInstigator();
    if (!HasAuthority() || bHitProcessed || bDeflected || !Character || !Character->IsAlive() ||
        !NewASC || !NewASC->HasMatchingGameplayTag(CCProjectileTags::CanDeflect) ||
        !UCC_TeamStatics::IsHostile(NewInstigator, PreviousShooter)) return false;

    FCC_DamageRequest Request;
    Request.SkillScale = Payload.SkillScale;
    Request.FlatDamage = Payload.FlatDamage;
    Request.DamageMultiplier = Payload.DamageMultiplier;
    Request.SourceTags = Payload.SourceTags;
    Request.SourceTags.AddTag(CCProjectileTags::DeflectedDamage);
    auto NewSnapshot = UCC_DamageStatics::PrepareDamage(NewASC, Request, this);
    if (!NewSnapshot.IsValid()) return false;
    // 仅换 Actor 归属不会更改旧 Spec 的来源；先准备完整的新快照，成功后再提交整个交接。
    auto Context = NewSnapshot.Data->GetContext();
    Context.AddInstigator(NewInstigator, this);
    FVector Direction = (PreviousShooter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    if (Direction.IsNearlyZero()) Direction = -GetActorForwardVector();
    const float Speed = FMath::Max(1.f, Movement->Velocity.Size() > 1.f ? Movement->Velocity.Size() : Movement->InitialSpeed);
    bDeflected = true;
    DamageSnapshot = NewSnapshot;
    Payload.SourceTags = Request.SourceTags;
    SetInstigator(NewInstigator);
    SetOwner(NewInstigator);
    Collision->IgnoreActorWhenMoving(PreviousShooter, false);
    Collision->IgnoreActorWhenMoving(NewInstigator, true);
    SetActorRotation(Direction.Rotation());
    Movement->Velocity = Direction * Speed;
    Movement->UpdateComponentVelocity();
    ForceNetUpdate();
    UE_LOG(LogCCBuff, Log, TEXT("[Arrow][Deflect] Arrow=%s Previous=%s New=%s"),
        *GetNameSafe(this), *GetNameSafe(PreviousShooter), *GetNameSafe(NewInstigator));
    return true;
}

void ACC_ArrowProjectile::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(bDrawCollisionDebug);
    // 即使出生点和射手胶囊相交，也不能让箭立即命中自己。
    Collision->IgnoreActorWhenMoving(GetInstigator(), true);
    Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnArrowHit);
    Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnArrowOverlap);
    UE_LOG(LogGAS_Demo, Log,
           TEXT("[BowAI][Arrow] BeginPlay: Arrow=%s Instigator=%s Location=%s Velocity=%s Authority=%d"),
           *GetNameSafe(this), *GetNameSafe(GetInstigator()), *GetActorLocation().ToCompactString(),
           *Movement->Velocity.ToCompactString(), HasAuthority());
}

void ACC_ArrowProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!CCDebug::DrawProjectile(bDrawCollisionDebug) || !IsValid(Collision))
    {
        return;
    }

    // 使用碰撞组件的世界位置和缩放后半径，而不是箭模型尺寸或固定的 8 cm。
    // DrawDebugSphere(GetWorld(), Collision->GetComponentLocation(), Collision->GetScaledSphereRadius(), 16, FColor::Cyan,
    //                 false, 0.0f, 0, 1.5f);
}

void ACC_ArrowProjectile::OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || bHitProcessed || !IsValid(OtherActor) || OtherActor == GetInstigator())
    {
        UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Arrow] Ignored hit: Arrow=%s Other=%s Authority=%d"),
               *GetNameSafe(this), *GetNameSafe(OtherActor), HasAuthority());
        return;
    }
    ProcessImpact(OtherActor, Hit);
}

void ACC_ArrowProjectile::OnArrowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                         const FHitResult& SweepResult)
{
    if (!HasAuthority() || bHitProcessed || !IsValid(OtherActor) || OtherActor == this ||
        OtherActor == GetInstigator())
    {
        return;
    }
    // 敌我统一由 IsHostile 判断（箭沿 Instigator 解析到射手，已包含魅惑）；非敌对的 Pawn 直接穿过。
    if (!UCC_TeamStatics::IsHostile(this, OtherActor))
    {
        UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Arrow] Pass through: Arrow=%s Other=%s"), *GetNameSafe(this),
               *GetNameSafe(OtherActor));
        return;
    }
    // 移动扫掠产生的重叠带有命中信息；其他情况（例如出生时就重叠）用箭的当前位置构造。
    const FHitResult Hit =
        bFromSweep ? SweepResult : FHitResult(OtherActor, OtherComp, GetActorLocation(), -GetActorForwardVector());
    ProcessImpact(OtherActor, Hit);
}

void ACC_ArrowProjectile::ProcessImpact(AActor* OtherActor, const FHitResult& Hit)
{
    /**
	 * 每支箭只结算第一次有效碰撞，防止同步回调再次提交伤害。
	 */
    bHitProcessed = true;
    if (CCDebug::DrawProjectile(bDrawCollisionDebug))
    {
        // 红圈是发生碰撞时球心的范围，黄点是表面接触点，便于检查命中偏移。
        DrawDebugSphere(GetWorld(), Hit.Location, Collision->GetScaledSphereRadius(), 16, FColor::Red, false, 2.0f, 0,
                        2.0f);
        DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Yellow, false, 2.0f);
    }
    UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Hit: Arrow=%s Instigator=%s Other=%s Impact=%s Damage=%.1f"),
           *GetNameSafe(this), *GetNameSafe(GetInstigator()), *GetNameSafe(OtherActor),
           *Hit.ImpactPoint.ToCompactString(), Payload.FlatDamage * Payload.DamageMultiplier);

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
    /**
	 * 命中只执行已保存的伤害快照；敌我关系统一查询，不读取任何具体 Buff。
	 * 世界障碍和友方碰撞仍会销毁箭，但不会向其提交伤害。
	 */
    if (IsValid(TargetASC) && UCC_TeamStatics::IsHostile(this, OtherActor))
    {
        // 先完成伤害与死亡结算，再通过目标自身的动作入口请求受击。
        // 入口统一检查死亡和 SuperArmor；不在箭中复制这些规则。
        const ECC_DamageApplyStatus Status = UCC_DamageStatics::ApplyPreparedDamage
            (TargetASC, DamageSnapshot, &Hit);
        bool bHitReactTriggered = false;
        if (Status == ECC_DamageApplyStatus::Applied && IsValid(OtherActor) && IsValid(TargetASC))
        {
            ACC_BaseCharacter* TargetCharacter = Cast<ACC_BaseCharacter>(OtherActor);
            UActionComponent* Actions = IsValid(TargetCharacter)
                ? TargetCharacter->GetUCombatActionComponent() : nullptr;
            // 使用快照中的来源 ASC，保证反弹后受击方向/来源属于当前伤害归属者。
            UAbilitySystemComponent* SourceASC = DamageSnapshot.IsValid()
                ? DamageSnapshot.Data->GetContext().GetOriginalInstigatorAbilitySystemComponent() : nullptr;
            if (IsValid(Actions) && IsValid(SourceASC))
            {
                bHitReactTriggered = Actions->TryGameplayAbilityByTag(CCTags::CCAbilityTrigger::BeHit,
                    SourceASC, TargetASC, UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit));
            }
        }
        UE_LOG(LogCCBuff, Log, TEXT("[Arrow][HitReact] Target=%s Triggered=%d"),
            *GetNameSafe(OtherActor), bHitReactTriggered);
        
        UE_LOG(LogCCBuff, Log, TEXT("[Arrow][Hit] Arrow=%s Target=%s DamageStatus=%d Ratio=%.3f"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor),
            static_cast<int32>(Status),
            Payload.DamageMultiplier);
    }
    else
    {
        UE_LOG(LogCCBuff, Verbose, TEXT("[Arrow][Hit] No hostile damage Arrow=%s Other=%s TargetASC=%s"),
               *GetNameSafe(this), *GetNameSafe(OtherActor), *GetNameSafe(TargetASC));
    }
    UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Destroy after hit: Arrow=%s"), *GetNameSafe(this));
    Destroy();
}
