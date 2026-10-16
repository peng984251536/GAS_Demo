#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/CC_DamageStatics.h"
#include "Damage/CC_DamageTags.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Effects/Damage/CC_GE_Damage.h"
#include "GameplayCues/CC_GameplayCueManager.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
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

	// 2. 伤害 GE 自带的 Cue：数值是 Execution 输出的理论伤害。
	FDamageTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	UWorld* World = TestWorld.World;
	APlayerController* Viewer = World->SpawnActor<APlayerController>();
	APlayerController* OtherViewer = World->SpawnActor<APlayerController>();
	World->AddController(Viewer);
	World->AddController(OtherViewer);
	UAbilitySystemComponent* Source = TestWorld.MakeASC();
	UAbilitySystemComponent* Target = TestWorld.MakeASC();
	Viewer->Possess(Cast<APawn>(Source->GetAvatarActor()));
	SetBase(Source, UCC_AttributeSet::GetAttackPowerAttribute(), 10.f);
	SetBase(Target, UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	UCC_DamageTextSubsystem* Text = World->GetSubsystem<UCC_DamageTextSubsystem>();
	if (!TestNotNull(TEXT("Damage text subsystem"), Text)) return false;
	TestTrue(TEXT("Project cue manager configured"), UAbilitySystemGlobals::Get().GetGameplayCueManager()->IsA<UCC_GameplayCueManager>());

	FCC_DamageRequest Request;
	Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("GE cue submits one entry to the attacker only"), Text->GetActiveEntries().Num(), 1);
	if (!Text->GetActiveEntries().IsEmpty())
	{
		TestTrue(TEXT("Entry belongs to attacker"), Text->GetActiveEntries()[0].Viewer == Viewer);
		TestEqual(TEXT("Cue magnitude is resolved damage"), Text->GetActiveEntries()[0].Amount, 10.f);
	}
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 3.f);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Overkill still shows resolved damage"), Text->GetActiveEntries()[0].Amount, 10.f);
	const int32 EntriesBeforeImmune = Text->GetActiveEntries().Num();
	SetBase(Target, UCC_AttributeSet::GetHealthAttribute(), 1000.f);
	Target->AddLooseGameplayTag(CCDamageTags::Immunity);
	UCC_DamageStatics::ApplyDamage(Source, Target, Request);
	TestEqual(TEXT("Immune hit shows no number"), Text->GetActiveEntries().Num(), EntriesBeforeImmune);
	Target->RemoveLooseGameplayTag(CCDamageTags::Immunity);

	// 3. 治疗 GE 自带的 Cue。
	UCC_DamageStatics::ApplyHealing(Source, Target, 5.f);
	TestTrue(TEXT("Heal cue uses heal style"), Text->GetActiveEntries()[0].Style == ECC_DamageTextStyle::Heal);
	Text->Tick(2.f);

	// 4. 飘字分组（表现层）：同组合并，目标/样式/观看者不同则独立。
	Text->MergeWindow = 0.25f;
	const FVector Location(100.f, 100.f, 100.f);
	AActor* FirstTarget = Target->GetAvatarActor();
	AActor* SecondTarget = World->SpawnActor<AActor>();
	AActor* SourceActor = Source->GetAvatarActor();
	Text->ReportHitForPlayer(Viewer, FirstTarget, SourceActor, Location, 10.f, ECC_DamageTextStyle::Normal, FGameplayTag());
	Text->ReportHitForPlayer(Viewer, FirstTarget, SourceActor, Location, 20.f, ECC_DamageTextStyle::Normal, FGameplayTag());
	TestEqual(TEXT("Same group merges"), Text->GetActiveEntries().Num(), 1);
	TestEqual(TEXT("Merged amount"), Text->GetActiveEntries()[0].Amount, 30.f);
	Text->ReportHitForPlayer(Viewer, SecondTarget, SourceActor, Location, 5.f, ECC_DamageTextStyle::Normal, FGameplayTag());
	Text->ReportHitForPlayer(Viewer, FirstTarget, SourceActor, Location, -5.f, ECC_DamageTextStyle::Heal, FGameplayTag());
	Text->ReportHitForPlayer(Viewer, FirstTarget, SourceActor, Location, 5.f, ECC_DamageTextStyle::Critical, FGameplayTag());
	Text->ReportHitForPlayer(OtherViewer, FirstTarget, SourceActor, Location, 5.f, ECC_DamageTextStyle::Normal, FGameplayTag());
	TestEqual(TEXT("Targets, styles and viewers stay independent"), Text->GetActiveEntries().Num(), 5);
	Text->Tick(2.f);
	return true;
}

#endif
