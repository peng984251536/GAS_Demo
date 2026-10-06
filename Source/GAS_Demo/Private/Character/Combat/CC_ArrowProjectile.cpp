#include "Character/Combat/CC_ArrowProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Damage/CC_LyraStyleDamage.h"
#include "Damage/GASDamageExecutionBase.h"
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
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	RootComponent = Collision;

	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMesh->SetupAttachment(Collision);
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

void ACC_ArrowProjectile::InitializeArrow(float InSpeed, float InFlatDamage)
{
	const float Speed = FMath::Max(1.0f, InSpeed);
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	//Movement->Velocity = GetActorForwardVector() * Speed;
	FlatDamage = FMath::Max(0.0f, InFlatDamage);
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Initialized: Arrow=%s Speed=%.1f Damage=%.1f Forward=%s"),
		*GetNameSafe(this), Speed, FlatDamage, *GetActorForwardVector().ToCompactString());
}

void ACC_ArrowProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(bDrawCollisionDebug);
	// 即使出生点和射手胶囊相交，也不能让箭立即命中自己。
	Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnArrowHit);
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] BeginPlay: Arrow=%s Instigator=%s Location=%s Velocity=%s Authority=%d"),
		*GetNameSafe(this), *GetNameSafe(GetInstigator()), *GetActorLocation().ToCompactString(),
		*Movement->Velocity.ToCompactString(), HasAuthority());
}

void ACC_ArrowProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDrawCollisionDebug || !IsValid(Collision))
	{
		return;
	}

	// 使用碰撞组件的世界位置和缩放后半径，而不是箭模型尺寸或固定的 8 cm。
	DrawDebugSphere(GetWorld(), Collision->GetComponentLocation(),
		Collision->GetScaledSphereRadius(), 16, FColor::Cyan, false, 0.0f, 0, 1.5f);
}

void ACC_ArrowProjectile::OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == GetInstigator())
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Arrow] Ignored hit: Arrow=%s Other=%s Authority=%d"),
			*GetNameSafe(this), *GetNameSafe(OtherActor), HasAuthority());
		return;
	}
	if (bDrawCollisionDebug)
	{
		// 红圈是发生碰撞时球心的范围，黄点是表面接触点，便于检查命中偏移。
		DrawDebugSphere(GetWorld(), Hit.Location, Collision->GetScaledSphereRadius(),
			16, FColor::Red, false, 2.0f, 0, 2.0f);
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Yellow, false, 2.0f);
	}
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Hit: Arrow=%s Instigator=%s Other=%s Impact=%s Damage=%.1f"),
		*GetNameSafe(this), *GetNameSafe(GetInstigator()), *GetNameSafe(OtherActor),
		*Hit.ImpactPoint.ToCompactString(), FlatDamage);

	// 用射手 ASC 创建 Spec，再作用于目标 ASC，才能正确捕获攻击力并归属伤害来源。
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetInstigator());
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (IsValid(SourceASC) && IsValid(TargetASC))
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(this);
		Context.AddHitResult(Hit);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCC_GE_LyraStyleDamage::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, FlatDamage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Damage effect submitted: Arrow=%s Target=%s FlatDamage=%.1f"),
				*GetNameSafe(this), *GetNameSafe(OtherActor), FlatDamage);
		}
		else
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Arrow] Damage spec creation failed: Arrow=%s Target=%s"),
				*GetNameSafe(this), *GetNameSafe(OtherActor));
		}
	}
	else
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Hit without damage: Arrow=%s Other=%s SourceASC=%s TargetASC=%s"),
			*GetNameSafe(this), *GetNameSafe(OtherActor), *GetNameSafe(SourceASC), *GetNameSafe(TargetASC));
	}
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Arrow] Destroy after hit: Arrow=%s"), *GetNameSafe(this));
	Destroy();
}
