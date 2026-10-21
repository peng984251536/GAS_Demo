#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Damage/CC_DamageStatics.h"
#include "Damage/CC_DamageTags.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "AbilitySystem/Effects/Damage/CC_GE_Damage.h"
#include "AbilitySystem/GameplayCues/CC_GameplayCueManager.h"
#include "UI/DamageText/CC_DamageNumberSource.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "CommonGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NativeGameplayTags.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/NameAsStringProxyArchive.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_FireDamage, "Damage.Type.Test.Fire");

namespace
{
	/** 隔离的游戏世界，测试结束自动销毁。 */
	struct FDamageTestWorld
	{
		UWorld* World = nullptr;
		FDamageTestWorld()
		{
			const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			if (World) GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
		}
		~FDamageTestWorld()
		{
			if (!World) return;
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UAbilitySystemComponent* MakeASC() const
		{
			APawn* Actor = World->SpawnActor<APawn>();
			UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Actor);
			Actor->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<UCC_AttributeSet>(Actor));
			ASC->InitAbilityActorInfo(Actor, Actor);
			return ASC;
		}
	};

	float Health(const UAbilitySystemComponent* ASC) { return ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()); }
	void SetBase(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float Value) { ASC->SetNumericAttributeBase(Attribute, Value); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCDamagePipelineTest, "GAS_Demo.Damage.Pipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCDamagePipelineTest::RunTest(const FString& Parameters)
{
	FDamageTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	UAbilitySystemComponent* Source = TestWorld.MakeASC();
	UAbilitySystemComponent* Target = TestWorld.MakeASC();
	const UCC_AttributeSet* Set = Target->GetSet<UCC_AttributeSet>();
	SetBase(Source, UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
	SetBase(Target, UCC_AttributeSet::GetMaxHealthAttribute(), 200.f);
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 200.f);

	// 1. 攻击 Buff 只算一次，且 Spec 保留创建时的攻击快照。
	UGameplayEffect* AttackBuff = NewObject<UGameplayEffect>(GetTransientPackage());
	AttackBuff->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& AttackMod = AttackBuff->Modifiers.AddDefaulted_GetRef();
	AttackMod.Attribute = UCC_AttributeSet::GetAttackPowerAttribute();
	AttackMod.ModifierOp = EGameplayModOp::MultiplyAdditive;
	AttackMod.ModifierMagnitude = FScalableFloat(1.5f);
	const FActiveGameplayEffectHandle BuffHandle = Source->ApplyGameplayEffectToSelf(AttackBuff, 1.f, Source->MakeEffectContext());
	TestEqual(TEXT("Buff raises attack to 150"), Source->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 150.f);

	FCC_DamageRequest Request;
	Request.SkillScale = 0.5f;
	Request.FlatDamage = 10.f;
	Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
	const FGameplayEffectSpecHandle Snapshot = UCC_DamageStatics::PrepareDamage(Source, Request);
	if (!TestTrue(TEXT("Snapshot created"), Snapshot.IsValid())) return false;
	SetBase(Source, UCC_AttributeSet::GetAttackPowerAttribute(), 200.f);
	TestTrue(TEXT("Prepared damage applied"), UCC_DamageStatics::ApplyPreparedDamage(Target, Snapshot) == ECC_DamageApplyStatus::Applied);
	TestEqual(TEXT("Snapshot damage 150*0.5+10=85"), Health(Target), 115.f);
	Source->RemoveActiveGameplayEffect(BuffHandle);
	UCC_DamageStatics::ApplyPreparedDamage(Target, Snapshot);
	TestEqual(TEXT("Snapshot survives buff removal"), Health(Target), 30.f);
	TestEqual(TEXT("Damage meta cleared"), Set->GetDamage(), 0.f);

	// 2. 完整公式：(100*2+20) * 1.5 * 暴击2 - 30 = 630，再 * 0.8 = 504。
	SetBase(Source, UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
	SetBase(Source, UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), 0.5f);
	SetBase(Source, UCC_AttributeSet::GetCriticalMultiplierAttribute(), 2.f);
	SetBase(Target, UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	SetBase(Target, UCC_AttributeSet::GetDefenseAttribute(), 30.f);
	SetBase(Target, UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), 0.8f);

	int32 TakenEvents = 0, DealtEvents = 0;
	FGameplayEventData LastTaken;
	const FDelegateHandle TakenHandle = Target->GenericGameplayEventCallbacks.FindOrAdd(CCDamageTags::EventDamageTaken)
		.AddLambda([&](const FGameplayEventData* Payload) { ++TakenEvents; LastTaken = *Payload; });
	const FDelegateHandle DealtHandle = Source->GenericGameplayEventCallbacks.FindOrAdd(CCDamageTags::EventDamageDealt)
		.AddLambda([&](const FGameplayEventData*) { ++DealtEvents; });
	ON_SCOPE_EXIT
	{
		Target->GenericGameplayEventCallbacks.FindOrAdd(CCDamageTags::EventDamageTaken).Remove(TakenHandle);
		Source->GenericGameplayEventCallbacks.FindOrAdd(CCDamageTags::EventDamageDealt).Remove(DealtHandle);
	};

	Request.SkillScale = 2.f;
	Request.FlatDamage = 20.f;
	Request.CriticalPolicy = ECC_CriticalPolicy::Forced;
	TestTrue(TEXT("ApplyDamage accepted"), UCC_DamageStatics::ApplyDamage(Source, Target, Request) == ECC_DamageApplyStatus::Applied);
	TestEqual(TEXT("Full formula 504"), Health(Target), 496.f);
	TestEqual(TEXT("One DamageTaken event"), TakenEvents, 1);
	TestEqual(TEXT("One DamageDealt event"), DealtEvents, 1);
	TestEqual(TEXT("Event magnitude is actual health damage"), LastTaken.EventMagnitude, 504.f);
	const FCC_GameplayEffectContext* EventContext = FCC_GameplayEffectContext::Get(LastTaken.ContextHandle);
	TestTrue(TEXT("Event context carries critical"), EventContext && EventContext->bCritical);

	// 3. 残血：事件给实际扣血，生命不为负。
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 5.f);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Overkill clamps health to 0"), Health(Target), 0.f);
	TestEqual(TEXT("Overkill event magnitude is 5"), LastTaken.EventMagnitude, 5.f);

	// 4. 已死亡目标：Execution 不输出，没有事件。
	const int32 EventsBeforeDead = TakenEvents;
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Dead target produces no event"), TakenEvents, EventsBeforeDead);

	// 5. 免疫：GE 的 Tag Requirements 拒绝应用。
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	Target->AddLooseGameplayTag(CCDamageTags::Immunity);
	TestTrue(TEXT("Immunity rejects the GE"), UCC_DamageStatics::ApplyDamage(Source, Target, Request) == ECC_DamageApplyStatus::Rejected);
	TestEqual(TEXT("Immunity keeps health"), Health(Target), 1000.f);
	Target->RemoveLooseGameplayTag(CCDamageTags::Immunity);

	// 6. 按伤害类型的条件修正：火抗 GE 只对带 Damage.Type.Test.Fire 的伤害生效。
	UGameplayEffect* FireResist = NewObject<UGameplayEffect>(GetTransientPackage());
	FireResist->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& ResistMod = FireResist->Modifiers.AddDefaulted_GetRef();
	ResistMod.Attribute = UCC_AttributeSet::GetIncomingDamageMultiplierAttribute();
	ResistMod.ModifierOp = EGameplayModOp::Additive;
	ResistMod.ModifierMagnitude = FScalableFloat(-0.4f);
	ResistMod.SourceTags.RequireTags.AddTag(TAG_Test_FireDamage);
	Target->ApplyGameplayEffectToSelf(FireResist, 1.f, Target->MakeEffectContext());
	Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
	Request.SkillScale = 0.f;
	Request.FlatDamage = 100.f;
	SetBase(Source, UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), 0.f);
	SetBase(Target, UCC_AttributeSet::GetDefenseAttribute(), 0.f);
	SetBase(Target, UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), 1.f);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Non-fire damage ignores fire resistance"), Health(Target), 900.f);
	Request.DamageType = TAG_Test_FireDamage;
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Fire damage uses tag-conditioned resistance (100*0.6)"), Health(Target), 840.f);
	TestTrue(TEXT("Event carries damage type tag"), LastTaken.InstigatorTags.HasTagExact(TAG_Test_FireDamage));

	// 7. 治疗与输入校验。
	UCC_DamageStatics::ApplyHealing(Source, Target, 2000.f);
	TestEqual(TEXT("Healing clamps to MaxHealth"), Health(Target), 1000.f);
	TestEqual(TEXT("Healing meta cleared"), Set->GetHealing(), 0.f);
	Request.FlatDamage = -1.f;
	TestTrue(TEXT("Negative input rejected"), UCC_DamageStatics::ApplyDamage(Source, Target, Request) == ECC_DamageApplyStatus::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCDamageCueTest, "GAS_Demo.Damage.CueAndContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCDamageCueTest::RunTest(const FString& Parameters)
{
	// 1. Context 只序列化暴击；HitResult 由基类按值复制。
	FCC_GameplayEffectContext Original;
	Original.bCritical = true;
	TArray<uint8> Bytes;
	FMemoryWriter MemoryWriter(Bytes);
	FNameAsStringProxyArchive Writer(MemoryWriter);
	bool bWriteOk = false;
	Original.NetSerialize(Writer, nullptr, bWriteOk);
	FMemoryReader MemoryReader(Bytes);
	FNameAsStringProxyArchive Reader(MemoryReader);
	FCC_GameplayEffectContext Received;
	bool bReadOk = false;
	Received.NetSerialize(Reader, nullptr, bReadOk);
	TestTrue(TEXT("Context serializes"), bWriteOk && bReadOk);
	TestTrue(TEXT("Critical survives serialization"), Received.bCritical);
	FHitResult Hit;
	Hit.ImpactPoint = FVector(20.f, 30.f, 40.f);
	Original.AddHitResult(Hit);
	const FGameplayEffectContextHandle Duplicate(Original.Duplicate());
	Hit.ImpactPoint = FVector::ZeroVector;
	Original.AddHitResult(Hit, true);
	TestEqual(TEXT("Duplicate copies HitResult by value"), Duplicate.GetHitResult()->ImpactPoint, FVector(20.f, 30.f, 40.f));

	// 2. 伤害 GE 自带的 Cue → 本地玩家的伤害数字来源：数值是 Execution 输出的理论伤害。
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("World"), World)) return false;
	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	UCommonGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	WorldContext.GameViewport = Viewport;
	Viewport->Init(WorldContext, Instance, false);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
	ON_SCOPE_EXIT
	{
		Instance->RemoveLocalPlayer(LocalPlayer);
		Instance->Shutdown();
		Viewport->DetachViewportClient();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	// 隔离世界没有 GameMode/登录流程，手动把控制器和本地玩家连起来。
	APlayerController* Viewer = World->SpawnActor<APlayerController>();
	Viewer->Player = LocalPlayer;
	LocalPlayer->PlayerController = Viewer;
	World->AddController(Viewer);
	auto MakeASC = [World]()
	{
		APawn* Actor = World->SpawnActor<APawn>();
		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Actor);
		Actor->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->AddAttributeSetSubobject(NewObject<UCC_AttributeSet>(Actor));
		ASC->InitAbilityActorInfo(Actor, Actor);
		return ASC;
	};
	UAbilitySystemComponent* Source = MakeASC();
	UAbilitySystemComponent* Target = MakeASC();
	Viewer->Possess(Cast<APawn>(Source->GetAvatarActor()));
	SetBase(Source, UCC_AttributeSet::GetAttackPowerAttribute(), 10.f);
	SetBase(Target, UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	UCC_DamageNumberSource* Numbers = LocalPlayer->GetSubsystem<UCC_DamageNumberSource>();
	UCC_WorldIndicatorSubsystem* Indicators = LocalPlayer->GetSubsystem<UCC_WorldIndicatorSubsystem>();
	if (!TestNotNull(TEXT("Damage number source"), Numbers) || !TestNotNull(TEXT("Indicator subsystem"), Indicators)) return false;
	TestTrue(TEXT("Project cue manager configured"), UAbilitySystemGlobals::Get().GetGameplayCueManager()->IsA<UCC_GameplayCueManager>());

	FCC_DamageRequest Request;
	Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("GE cue shows one number"), Numbers->GetActiveCount(), 1);
	TestEqual(TEXT("Number is a world indicator"), Indicators->GetIndicators().Num(), 1);
	if (Numbers->GetActiveCount() > 0)
	{
		TestEqual(TEXT("Cue magnitude is resolved damage"), Numbers->GetActiveNumbers().Last()->Amount, 10.f);
		TestTrue(TEXT("Number stays at a fixed location"), Numbers->GetActiveNumbers().Last()->GetAnchorActor() == nullptr);
	}
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 3.f);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Overkill still shows resolved damage"), Numbers->GetActiveNumbers().Last()->Amount, 10.f);
	TestTrue(TEXT("Second number on the same target is offset"),
		Numbers->GetActiveNumbers().Last()->ScreenOffset != Numbers->GetActiveNumbers()[0]->ScreenOffset);
	const int32 CountBeforeImmune = Numbers->GetActiveCount();
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	Target->AddLooseGameplayTag(CCDamageTags::Immunity);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Immune hit shows no number"), Numbers->GetActiveCount(), CountBeforeImmune);
	Target->RemoveLooseGameplayTag(CCDamageTags::Immunity);

	// 3. 治疗 GE 自带的 Cue。
	UCC_DamageStatics::ApplyHealing(Source, Target, 5.f);
	TestTrue(TEXT("Heal cue uses heal style"), Numbers->GetActiveNumbers().Last()->Style == ECC_DamageTextStyle::Heal);

	// 4. 移除后指示器对象被回收，再显示时复用。
	UCC_DamageNumberIndicator* First = Numbers->GetActiveNumbers()[0];
	Indicators->RemoveIndicator(First);
	TestEqual(TEXT("Removed number leaves the source"), Numbers->GetActiveCount(), CountBeforeImmune);
	Numbers->ShowNumber(Target->GetAvatarActor(), FVector::ZeroVector, 7.f, ECC_DamageTextStyle::Normal, FGameplayTag());
	TestTrue(TEXT("Indicator object is recycled"), Numbers->GetActiveNumbers().Last() == First);
	Indicators->ClearIndicators();
	TestEqual(TEXT("Clearing indicators empties the source"), Numbers->GetActiveCount(), 0);
	return true;
}

#endif
