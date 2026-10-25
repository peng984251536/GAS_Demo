#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "AbilitySystem/Effects/Buff/CC_GE_NewOffers.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Actors/Projectiles/CC_ProjectileInteractions.h"
#include "Animation/Notifies/AnimNotify_MeleeHitBox.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CC_BuffComponent.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Damage/CC_DamageTags.h"
#include "GameplayTags/CC_Tags.h"
#include "GameFramework/ProjectileMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffDeflectTest, "GAS_Demo.Buff.NewOffers.Deflect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCBuffDeflectTest::RunTest(const FString& Parameters)
{
    // 测试 World 不启动完整游戏；允许动态碰撞委托通过 Actor::ProcessEvent 派发到原生回调。
    FEditorScriptExecutionGuard ScriptExecution;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto MakeCharacter = [World](FVector Location, FGameplayTag Team)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* C = World->SpawnActor<ACC_EnemyCharacter>(Location, FRotator::ZeroRotator, Params);
        auto* ASC = C->GetAbilitySystemComponent();
        ASC->AddAttributeSetSubobject(C->GetAttributeSet());
        ASC->InitAbilityActorInfo(C, C);
        C->GetRuntimeData()->SetAlive(true);
        C->GetBuffComponent()->InitializeASC(ASC, C->GetAttributeSet());
        C->SetTeamOverride(Team);
        ASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 500.f);
        ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 500.f);
        ASC->SetNumericAttributeBase(UCC_AttributeSet::GetCriticalChanceAttribute(), 0.f);
        return C;
    };
    auto* Defender = MakeCharacter(FVector::ZeroVector, CCTags::Team::Player);
    auto* Shooter = MakeCharacter(FVector(1000, 0, 0), CCTags::Team::Enemy);
    auto* ASC = Defender->GetAbilitySystemComponent();
    auto* EnemyASC = Shooter->GetAbilitySystemComponent();
    auto* Buffs = Defender->GetBuffComponent();
    auto* DeflectClass = LoadClass<UGameplayEffect>(nullptr,
        TEXT("/Game/GAS_Demo/GameplayEffects/NewBuff/GE_Buff_DeflectArrow.GE_Buff_DeflectArrow_C"));
    if (!TestNotNull(TEXT("Saved deflect blueprint"), DeflectClass)) return false;
    const FVector ArrowLocation(200, 0, 0);
    auto MakeArrow = [&](APawn* Owner)
    {
        const FTransform Transform(FRotator(0, 180, 0), ArrowLocation);
        auto* Arrow = World->SpawnActorDeferred<ACC_ArrowProjectile>(ACC_ArrowProjectile::StaticClass(),
            Transform, Owner, Owner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        FCC_ArrowPayload Payload;
        Payload.SourceTags.AddTag(CCBuffTags::ProjectileDamage);
        TestTrue(TEXT("Initial snapshot prepared"), Arrow->InitializeArrow(1800.f, Payload));
        Arrow->FinishSpawning(Transform);
        Arrow->DispatchBeginPlay();
        return Arrow;
    };
    auto* Arrow = MakeArrow(Shooter);
    auto Query = [&]() { return CCProjectileInteractions::DeflectInShape(Defender, ArrowLocation,
        FQuat::Identity, FCollisionShape::MakeSphere(100.f)); };
    ASC->AddLooseGameplayTag(CCTags::CCAbilities::Attack);
    TestEqual(TEXT("No qualification cannot deflect"), Query(), 0);
    auto Handle = Buffs->ApplyBuff(DeflectClass);
    TestTrue(TEXT("Qualification granted"), ASC->HasMatchingGameplayTag(CCProjectileTags::CanDeflect));
    ASC->RemoveLooseGameplayTag(CCTags::CCAbilities::Attack);
    TestEqual(TEXT("Idle actor cannot deflect"), Query(), 0);
    ASC->AddLooseGameplayTag(CCTags::CCAbilities::Attack);
    Handle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Handle), true, false);
    TestEqual(TEXT("Inhibition blocks deflection"), Query(), 0);
    Handle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Handle), false, false);
    Defender->GetRuntimeData()->SetAlive(false);
    TestFalse(TEXT("Dead actor cannot deflect despite retained qualification"), Arrow->Deflect(Defender));
    Defender->GetRuntimeData()->SetAlive(true);
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 20.f);
    EnemyASC->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 200.f);
    auto Attack = Buffs->ApplyBuff(UCC_GE_AttackPermanent::StaticClass());
    auto* FriendlyArrow = MakeArrow(Defender);
    TestFalse(TEXT("Friendly arrow rejected"), FriendlyArrow->Deflect(Defender));
    FriendlyArrow->Destroy();

    // 验证实际 Notify 的有效帧入口，使用它配置的形状而非全局扫描。
    auto* Notify = NewObject<UAnimNotify_MeleeHitBox>();
    Notify->Shapes.SetNum(1);
    Notify->Shapes[0].ShapeType = ECC_HitShapeType::Sphere;
    Notify->Shapes[0].Radius = 100.f;
    Notify->Shapes[0].Offset = Defender->GetMesh()->GetComponentTransform().InverseTransformPosition(ArrowLocation);
    Notify->Notify(Defender->GetMesh(), nullptr, FAnimNotifyEventReference());
    TestTrue(TEXT("Notify deflects enemy arrow"), Arrow->GetInstigator() == Defender);
    TestTrue(TEXT("Ownership transferred"), Arrow->GetOwner() == Defender);
    TestTrue(TEXT("Velocity points at original shooter"),
        Arrow->FindComponentByClass<UProjectileMovementComponent>()->Velocity.X > 0.f);
    TestEqual(TEXT("Repeated query does not reflect twice"), Query(), 0);
    auto EnemyHandle = Shooter->GetBuffComponent()->ApplyBuff(DeflectClass);
    TestFalse(TEXT("Cannot ping-pong"), Arrow->Deflect(Shooter));
    ASC->RemoveActiveGameplayEffect(Attack);
    ASC->RemoveActiveGameplayEffect(Handle);
    TestFalse(TEXT("Qualification removed"), ASC->HasMatchingGameplayTag(CCProjectileTags::CanDeflect));
    auto* Next = MakeArrow(Shooter);
    TestFalse(TEXT("Future arrows cannot be deflected after removal"), Next->Deflect(Defender));
    Next->Destroy();
    auto Charm = Buffs->ApplyBuff(UCC_GE_CharmAttackTen::StaticClass());

    int32 DefenderEvents = 0, EnemyEvents = 0;
    const FGameplayTagContainer Tags(CCDamageTags::EventDamageDealt);
    const auto NewListener = ASC->AddGameplayEventTagContainerDelegate(Tags,
        FGameplayEventTagMulticastDelegate::FDelegate::CreateLambda([&](FGameplayTag, const FGameplayEventData* Event)
        {
            ++DefenderEvents;
            TestTrue(TEXT("Damage instigator is deflector"), Event->Instigator.Get() == Defender);
            TestTrue(TEXT("Deflected source tag"), Event->InstigatorTags.HasTag(CCProjectileTags::DeflectedDamage));
            TestTrue(TEXT("Context cause is arrow"), Event->ContextHandle.GetEffectCauser() == Arrow);
        }));
    const auto OldListener = EnemyASC->AddGameplayEventTagContainerDelegate(Tags,
        FGameplayEventTagMulticastDelegate::FDelegate::CreateLambda([&](FGameplayTag, const FGameplayEventData*) { ++EnemyEvents; }));
    // 通过箭已绑定的碰撞回调执行真实伤害管线；父 Buff 已移除，但反弹快照仍独立有效。
    auto* Collision = Arrow->FindComponentByClass<USphereComponent>();
    TestTrue(TEXT("Impact callback bound"), Collision->OnComponentHit.IsBound());
    const FHitResult Hit(Shooter, Cast<UPrimitiveComponent>(Shooter->GetRootComponent()), Shooter->GetActorLocation(), -FVector::ForwardVector);
    Collision->OnComponentHit.Broadcast(Collision, Shooter, Cast<UPrimitiveComponent>(Shooter->GetRootComponent()), FVector::ZeroVector, Hit);
    TestEqual(TEXT("Damage retains deflection-time attack snapshot (40+10)"), Shooter->GetAttributeSet()->GetHealth(), 450.f);
    TestEqual(TEXT("Deflector receives damage event"), DefenderEvents, 1);
    TestEqual(TEXT("Original shooter receives no dealt event"), EnemyEvents, 0);
    TestTrue(TEXT("Deflected damage composes with charm attack"), EnemyASC->HasMatchingGameplayTag(CCTags::Status::Charmed));
    ASC->RemoveActiveGameplayEffect(Charm);
    ASC->RemoveGameplayEventTagContainerDelegate(Tags, NewListener);
    EnemyASC->RemoveGameplayEventTagContainerDelegate(Tags, OldListener);
    EnemyASC->RemoveActiveGameplayEffect(EnemyHandle);
    ASC->RemoveLooseGameplayTag(CCTags::CCAbilities::Attack);
    return true;
}
#endif
