#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Components/CC_BuffSelectionComponent.h"
#include "Data/Buff/CC_BuffOfferPool.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Survival/CC_StatTagStack.h"
#include "Game/Survival/CC_SurvivalModeConfig.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTags/CC_Tags.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/CC_GameEventSubsystem.h"
#include "Tests/CC_BuffSelectionTestTypes.h"
#include "Tests/CC_BuffTestTypes.h"
#include "Tests/CC_SurvivalTestTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

namespace CCSurvivalTest
{
	FCC_WaveTier MakeTier(int32 StartWave, int32 BaseCount, float CountPerWave)
	{
		FCC_WaveTier Tier;
		Tier.StartWave = StartWave;
		Tier.BaseCount = BaseCount;
		Tier.CountPerWave = CountPerWave;
		return Tier;
	}

	/** 测试世界：不执行 BeginPlay，调用方自己初始化 ASC。 */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
			if (World)
			{
				GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
			}
		}
		~FTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	/** 生成一只已初始化 ASC 的测试敌人。 */
	ACC_SurvivalTestEnemy* SpawnEnemy(UWorld* World)
	{
		ACC_SurvivalTestEnemy* Enemy = World->SpawnActor<ACC_SurvivalTestEnemy>();
		if (!Enemy)
		{
			return nullptr;
		}
		UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(Enemy->GetAttributeSet());
		ASC->InitAbilityActorInfo(Enemy, Enemy);
		Enemy->GetRuntimeData()->SetAlive(true);
		Enemy->GetBuffComponent()->InitializeASC(ASC, Enemy->GetAttributeSet());
		return Enemy;
	}

	/** 直接把 Health 覆盖为指定值的瞬时 GE（运行时构造，只用于测试）。 */
	UGameplayEffect* MakeSetHealthEffect(float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UCC_AttributeSet::GetHealthAttribute();
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		Effect->Modifiers.Add(Modifier);
		return Effect;
	}

	void ApplyToSelf(UAbilitySystemComponent* ASC, const UGameplayEffect* Effect)
	{
		const FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.f);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}
}

// ---------------------------------------------------------------------------
// 纯计算
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalTierLookupTest, "GAS_Demo.Survival.Wave.TierLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCSurvivalTierLookupTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	FCC_WaveSettings Settings;
	TestNull(TEXT("没有档位返回空"), UCC_SurvivalModeConfig::FindTier(Settings, 1));
	Settings.Tiers = { MakeTier(1, 5, 1.f), MakeTier(6, 8, 1.f), MakeTier(11, 12, 2.f) };
	TestEqual(TEXT("第 1 波用第一档"), UCC_SurvivalModeConfig::FindTier(Settings, 1)->StartWave, 1);
	TestEqual(TEXT("第 5 波仍是第一档"), UCC_SurvivalModeConfig::FindTier(Settings, 5)->StartWave, 1);
	TestEqual(TEXT("第 6 波进入第二档"), UCC_SurvivalModeConfig::FindTier(Settings, 6)->StartWave, 6);
	TestEqual(TEXT("第 11 波进入第三档"), UCC_SurvivalModeConfig::FindTier(Settings, 11)->StartWave, 11);
	TestEqual(TEXT("超过最后一档沿用最后一档"), UCC_SurvivalModeConfig::FindTier(Settings, 99)->StartWave, 11);
	TestEqual(TEXT("第 0 波兜底第一档"), UCC_SurvivalModeConfig::FindTier(Settings, 0)->StartWave, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalCountTest, "GAS_Demo.Survival.Wave.Count",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCSurvivalCountTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	const FCC_WaveTier Tier = MakeTier(6, 8, 1.5f);
	TestEqual(TEXT("档内第一波 = BaseCount"), UCC_SurvivalModeConfig::ComputeCount(Tier, 6, 1.f), 8);
	TestEqual(TEXT("档内第三波 = 8 + 1.5 × 2"), UCC_SurvivalModeConfig::ComputeCount(Tier, 8, 1.f), 11);
	TestEqual(TEXT("人数倍率 2 翻倍"), UCC_SurvivalModeConfig::ComputeCount(Tier, 6, 2.f), 16);
	TestEqual(TEXT("倍率 0 不刷"), UCC_SurvivalModeConfig::ComputeCount(Tier, 6, 0.f), 0);
	TestEqual(TEXT("负倍率按 0 处理"), UCC_SurvivalModeConfig::ComputeCount(Tier, 6, -1.f), 0);

	FRuntimeFloatCurve Empty;
	TestEqual(TEXT("空曲线视为 1"), UCC_SurvivalModeConfig::EvaluateMultiplier(Empty, 3.f), 1.f);
	FRuntimeFloatCurve Curve;
	Curve.GetRichCurve()->AddKey(1.f, 1.f);
	Curve.GetRichCurve()->AddKey(4.f, 2.5f);
	TestEqual(TEXT("曲线端点"), UCC_SurvivalModeConfig::EvaluateMultiplier(Curve, 4.f), 2.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalLevelUpTest, "GAS_Demo.Survival.Progression.LevelUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCSurvivalLevelUpTest::RunTest(const FString& Parameters)
{
	FCC_ProgressionSettings Settings;
	Settings.BaseExperienceToNext = 20.f;     // 1→2: 20
	Settings.ExperienceToNextPerLevel = 10.f; // 2→3: 30, 3→4: 40
	int32 Level = 1;
	float Experience = 0.f;
	TestEqual(TEXT("不够升级"), UCC_SurvivalModeConfig::ApplyExperience(Settings, Level, Experience, 15.f), 0);
	TestEqual(TEXT("经验累积"), Experience, 15.f);
	TestEqual(TEXT("一次连升两级"), UCC_SurvivalModeConfig::ApplyExperience(Settings, Level, Experience, 40.f), 2);
	TestEqual(TEXT("等级 3"), Level, 3);
	TestEqual(TEXT("剩余经验 = 55 - 20 - 30"), Experience, 5.f);
	TestEqual(TEXT("非法经验忽略"), UCC_SurvivalModeConfig::ApplyExperience(Settings, Level, Experience, -10.f), 0);

	Settings.MaxLevel = 4;
	TestEqual(TEXT("到上限只升到 4"), UCC_SurvivalModeConfig::ApplyExperience(Settings, Level, Experience, 1000.f), 1);
	TestEqual(TEXT("停在上限"), Level, 4);
	TestEqual(TEXT("满级经验清零"), Experience, 0.f);
	TestEqual(TEXT("满级后不再升级"), UCC_SurvivalModeConfig::ApplyExperience(Settings, Level, Experience, 1000.f), 0);

	FCC_ProgressionSettings CurveSettings;
	CurveSettings.ExperienceToNext.GetRichCurve()->AddKey(1.f, 5.f);
	CurveSettings.ExperienceToNext.GetRichCurve()->AddKey(10.f, 50.f);
	TestEqual(TEXT("曲线优先于线性公式"), UCC_SurvivalModeConfig::ComputeExperienceToNext(CurveSettings, 1), 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalRespawnDelayTest, "GAS_Demo.Survival.Respawn.Delay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCSurvivalRespawnDelayTest::RunTest(const FString& Parameters)
{
	FCC_RespawnSettings Settings;
	Settings.BaseDelay = 5.f;
	Settings.DelayPerDeath = 2.f;
	Settings.MaxDelay = 10.f;
	TestEqual(TEXT("第一次阵亡 = BaseDelay"), UCC_SurvivalModeConfig::ComputeRespawnDelay(Settings, 1), 5.f);
	TestEqual(TEXT("第三次阵亡 = 5 + 2 × 2"), UCC_SurvivalModeConfig::ComputeRespawnDelay(Settings, 3), 9.f);
	TestEqual(TEXT("封顶"), UCC_SurvivalModeConfig::ComputeRespawnDelay(Settings, 20), 10.f);
	TestEqual(TEXT("次数 0 按第一次"), UCC_SurvivalModeConfig::ComputeRespawnDelay(Settings, 0), 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalStatsTest, "GAS_Demo.Survival.Stats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCSurvivalStatsTest::RunTest(const FString& Parameters)
{
	FCC_StatTagStackContainer Stats;
	TestEqual(TEXT("不存在为 0"), Stats.GetStackCount(CCSurvivalTags::StatKills), 0);
	Stats.AddStack(CCSurvivalTags::StatKills, 3);
	Stats.AddStack(CCSurvivalTags::StatKills, 2);
	Stats.AddStack(CCSurvivalTags::StatDeaths, 1);
	TestEqual(TEXT("累加"), Stats.GetStackCount(CCSurvivalTags::StatKills), 5);
	TestEqual(TEXT("不同标签独立"), Stats.GetStackCount(CCSurvivalTags::StatDeaths), 1);
	Stats.AddStack(CCSurvivalTags::StatDeaths, -5);
	TestEqual(TEXT("不低于 0"), Stats.GetStackCount(CCSurvivalTags::StatDeaths), 0);
	Stats.AddStack(FGameplayTag(), 3);
	TestEqual(TEXT("无效标签忽略"), Stats.GetStacks().Num(), 2);
	Stats.Reset();
	TestEqual(TEXT("清空"), Stats.GetStackCount(CCSurvivalTags::StatKills), 0);
	return true;
}

// ---------------------------------------------------------------------------
// 死亡事件
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalOutOfHealthTest, "GAS_Demo.Survival.Death.OutOfHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 生命耗尽只广播一次、带来源；回血后可再次广播。 */
bool FCCSurvivalOutOfHealthTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	ACC_SurvivalTestEnemy* Enemy = SpawnEnemy(TestWorld.World);
	if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	UCC_AttributeSet* Attributes = Enemy->GetAttributeSet();

	int32 Broadcasts = 0;
	AActor* LastInstigator = nullptr;
	const FDelegateHandle Handle = Attributes->OnOutOfHealth.AddLambda(
		[&](AActor* Instigator, AActor*, const FGameplayEffectSpec&, float)
		{
			++Broadcasts;
			LastInstigator = Instigator;
		});
	ON_SCOPE_EXIT { Attributes->OnOutOfHealth.Remove(Handle); };

	TStrongObjectPtr<UGameplayEffect> Kill(MakeSetHealthEffect(0.f));
	TStrongObjectPtr<UGameplayEffect> Heal(MakeSetHealthEffect(50.f));
	ApplyToSelf(ASC, Kill.Get());
	TestEqual(TEXT("归零广播一次"), Broadcasts, 1);
	TestTrue(TEXT("处于生命耗尽状态"), Attributes->IsOutOfHealth());
	TestTrue(TEXT("来源是施加者的 Avatar"), LastInstigator == static_cast<AActor*>(Enemy));
	ApplyToSelf(ASC, Kill.Get());
	TestEqual(TEXT("补刀不再广播"), Broadcasts, 1);
	ApplyToSelf(ASC, Heal.Get());
	TestFalse(TEXT("回血后解除"), Attributes->IsOutOfHealth());
	ApplyToSelf(ASC, Kill.Get());
	TestEqual(TEXT("再次归零再次广播"), Broadcasts, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSurvivalDeathBroadcastTest, "GAS_Demo.Survival.Death.BroadcastOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 角色把生命耗尽转为一次 OnCharacterDied；死亡清理幂等；补刀不重复广播。 */
bool FCCSurvivalDeathBroadcastTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	UCC_GameEventSubsystem* Events = UCC_GameEventSubsystem::Get(TestWorld.World);
	if (!TestNotNull(TEXT("GameEventSubsystem"), Events)) return false;
	ACC_SurvivalTestEnemy* Enemy = SpawnEnemy(TestWorld.World);
	if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
	Enemy->TestBindOutOfHealth();

	int32 Deaths = 0;
	FCC_DeathInfo LastInfo;
	const FDelegateHandle Handle = Events->OnCharacterDied.AddLambda([&](const FCC_DeathInfo& Info)
	{
		++Deaths;
		LastInfo = Info;
	});
	ON_SCOPE_EXIT { Events->OnCharacterDied.Remove(Handle); };

	// 模拟死亡能力先于生命耗尽事件完成清理：广播仍然要发生。
	Enemy->ACC_BaseCharacter::HandleDeath();
	TestFalse(TEXT("死亡清理已执行"), Enemy->IsAlive());
	Enemy->ACC_BaseCharacter::HandleDeath();

	TStrongObjectPtr<UGameplayEffect> Kill(MakeSetHealthEffect(0.f));
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ApplyToSelf(ASC, Kill.Get());
	TestEqual(TEXT("死亡广播一次"), Deaths, 1);
	TestTrue(TEXT("Victim 是自己"), LastInfo.Victim.Get() == static_cast<ACC_BaseCharacter*>(Enemy));
	TestTrue(TEXT("Killer 是 GE 来源"), LastInfo.Killer.Get() == static_cast<AActor*>(Enemy));
	TestTrue(TEXT("非玩家击杀者没有 PlayerState"), LastInfo.KillerPlayerState.Get() == nullptr);

	ApplyToSelf(ASC, Kill.Get());
	TestEqual(TEXT("补刀不重复广播"), Deaths, 1);
	return true;
}

// ---------------------------------------------------------------------------
// Buff：复活恢复挂起实例（与 ClearBuffs(OwnerDeath) 对称）
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffResumeAfterReviveTest, "GAS_Demo.Buff.ResumeAfterRevive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCBuffResumeAfterReviveTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	ACC_SurvivalTestEnemy* Character = SpawnEnemy(TestWorld.World);
	if (!TestNotNull(TEXT("Character"), Character)) return false;
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	UCC_BuffComponent* Component = Character->GetBuffComponent();
	auto FindBehavior = [Component](FActiveGameplayEffectHandle Handle)
	{
		UCC_BuffTestBehavior* Found = nullptr;
		ForEachObjectWithOuter(Component, [&Found, Handle](UObject* Object)
		{
			auto* Behavior = Cast<UCC_BuffTestBehavior>(Object);
			if (Behavior && Behavior->GetContext().Handle == Handle) Found = Behavior;
		}, false);
		return Found;
	};
	auto SendEvent = [ASC]
	{
		FGameplayEventData Event;
		ASC->HandleGameplayEvent(CCTags::CCAbilityTrigger::BowShoot, &Event);
	};

	const FActiveGameplayEffectHandle Retained = Component->ApplyBuff(UCC_BuffTestRetainedEffect::StaticClass());
	TStrongObjectPtr<UCC_BuffTestBehavior> Behavior(FindBehavior(Retained));
	if (!TestNotNull(TEXT("保留型实例"), Behavior.Get())) return false;
	const FActiveGameplayEffectHandle Removed = Component->ApplyBuff(UCC_BuffTestOtherEffect::StaticClass());
	TestTrue(TEXT("普通 Buff 已施加"), Removed.IsValid());
	const int32 EnablesBefore = Behavior->Enables;

	Character->ACC_BaseCharacter::HandleDeath();
	TestEqual(TEXT("死亡只保留配置为保留的 GE"), Component->GetActiveBuffs().Num(), 1);
	TestFalse(TEXT("尸体上的规则暂停"), Behavior->IsEnabled());
	SendEvent();
	const int32 EventsWhileDead = Behavior->Events;

	Character->GetRuntimeData()->SetAlive(true);
	Component->ResumeAfterRevive();
	TestTrue(TEXT("复活后规则恢复"), Behavior->IsEnabled());
	TestEqual(TEXT("同一实例重新启用一次"), Behavior->Enables, EnablesBefore + 1);
	TestEqual(TEXT("不重新初始化"), Behavior->Initializes, 1);
	SendEvent();
	TestEqual(TEXT("恢复事件订阅"), Behavior->Events, EventsWhileDead + 1);
	Component->ResumeAfterRevive();
	TestEqual(TEXT("重复调用不重复启用"), Behavior->Enables, EnablesBefore + 1);

	// 死亡时被抑制的实例：复活后仍按 GAS 状态保持禁用。
	Character->ACC_BaseCharacter::HandleDeath();
	ASC->AddLooseGameplayTag(CCTags::Status::Invincible);
	Character->GetRuntimeData()->SetAlive(true);
	Component->ResumeAfterRevive();
	TestFalse(TEXT("仍被抑制时不启用"), Behavior->IsEnabled());
	ASC->RemoveLooseGameplayTag(CCTags::Status::Invincible);
	TestTrue(TEXT("抑制解除后启用"), Behavior->IsEnabled());

	Component->ClearBuffs(ECC_BuffEndReason::EndPlay);
	TestEqual(TEXT("最终清理"), Component->GetActiveBuffs().Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Buff 三选一：服务器事件（选卡保护、统计使用）
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffSelectionServerEventsTest, "GAS_Demo.Buff.Selection.ServerEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCBuffSelectionServerEventsTest::RunTest(const FString& Parameters)
{
	using namespace CCSurvivalTest;
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("World"), TestWorld.World)) return false;
	APlayerController* Player = TestWorld.World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("PlayerController"), Player)) return false;
	UCC_BuffSelectionTestComponent* Selection = NewObject<UCC_BuffSelectionTestComponent>(Player);
	Selection->RegisterComponent();

	TStrongObjectPtr<UCC_BuffOfferPool> Pool(NewObject<UCC_BuffOfferPool>());
	FCC_BuffOfferOption Option;
	Option.Effect = UCC_BuffTestEffect::StaticClass();
	Pool->Entries = { Option };
	Pool->OfferCount = 1;

	TArray<bool> PendingChanges;
	int32 Accepted = 0;
	Selection->OnServerPendingChanged.AddLambda([&](bool bPending) { PendingChanges.Add(bPending); });
	Selection->OnServerChoiceAccepted.AddLambda([&](const FCC_BuffOfferOption&) { ++Accepted; });

	Selection->OfferBuffs(Pool.Get());
	Selection->OfferBuffs(Pool.Get());
	TestEqual(TEXT("无 → 有 广播一次（排队不广播）"), PendingChanges.Num(), 1);
	TestTrue(TEXT("第一条为 true"), PendingChanges.Num() > 0 && PendingChanges[0]);

	Selection->NextResult = ECC_BuffChoiceResult::ApplyFailed;
	Selection->Resolve(1, 0);
	TestEqual(TEXT("失败不算接受"), Accepted, 0);
	Selection->NextResult = ECC_BuffChoiceResult::Accepted;
	Selection->Resolve(1, 0);
	TestEqual(TEXT("接受一次"), Accepted, 1);
	TestEqual(TEXT("接上排队的一轮时不广播"), PendingChanges.Num(), 1);
	Selection->Resolve(2, 0);
	TestEqual(TEXT("全部选完 有 → 无"), PendingChanges.Num(), 2);
	TestTrue(TEXT("第二条为 false"), PendingChanges.Num() > 1 && !PendingChanges[1]);

	Selection->OfferBuffs(Pool.Get());
	Selection->ClearOffers();
	TestEqual(TEXT("清空也广播 有 → 无"), PendingChanges.Num(), 4);
	return true;
}
#endif
