#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/CC_TeamTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "Character/CC_EnemyCharacter.h"
#include "Character/CC_PlayerCharacter.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GameplayTags/CC_Tags.h"
#include "Player/CC_AIController.h"
#include "Team/CC_TeamStatics.h"

namespace CCTeamTests
{
/**
 * 创建一个不开始游戏的测试 World，并注册 WorldContext；调用方负责用 DestroyWorld 清理。
 * @return 新建的 World；创建失败返回 nullptr。
 */
UWorld* CreateTestWorld()
{
	const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	if (World)
	{
		GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	}
	return World;
}

void DestroyTestWorld(UWorld* World)
{
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
}

/**
 * 在指定位置生成一个存活的角色。测试 World 不调用 BeginPlay，存活状态需要手动设置。
 * @param World 测试 World。
 * @param Location 生成位置。
 * @return 生成的角色。
 */
template<typename TCharacter>
TCharacter* SpawnAlive(UWorld* World, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TCharacter* Character = World->SpawnActor<TCharacter>(Location, FRotator::ZeroRotator, Params);
	if (Character)
	{
		Character->GetRuntimeData()->SetAlive(true);
	}
	return Character;
}

/**
 * 以 Charmer 为施加者，给敌人施加测试魅惑效果。
 * @param Target 被魅惑的敌人，其 ASC 会在这里完成初始化。
 * @param Charmer 魅惑者，写入 GE Context 的 Instigator。
 * @return 活动效果句柄，用于之后移除。
 */
FActiveGameplayEffectHandle ApplyCharm(ACC_EnemyCharacter* Target, AActor* Charmer)
{
	UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Target, Target);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Charmer, Charmer);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UCC_TeamTestCharmEffect::StaticClass(), 1.f, Context);
	return Spec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCTeamHostilityTest, "GAS_Demo.Team.HostilityAndCharm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 验证基础阵营、敌对判断和魅惑后的有效阵营。
 * 玩家角色在测试中没有 PlayerState，因此没有 ASC 和配置，走角色类默认阵营。
 */
bool FCCTeamHostilityTest::RunTest(const FString& Parameters)
{
	using namespace CCTeamTests;
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("World"), World)) return false;
	ON_SCOPE_EXIT { DestroyTestWorld(World); };

	auto* EnemyA = SpawnAlive<ACC_EnemyCharacter>(World, FVector(0.f, 0.f, 0.f));
	auto* EnemyB = SpawnAlive<ACC_EnemyCharacter>(World, FVector(300.f, 0.f, 0.f));
	auto* Player = SpawnAlive<ACC_PlayerCharacter>(World, FVector(600.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("EnemyA"), EnemyA) || !TestNotNull(TEXT("EnemyB"), EnemyB) || !TestNotNull(TEXT("Player"), Player))
		return false;

	TestEqual(TEXT("敌人默认阵营"), UCC_TeamStatics::GetBaseTeam(EnemyA), CCTags::Team::Enemy.GetTag());
	TestEqual(TEXT("玩家默认阵营"), UCC_TeamStatics::GetBaseTeam(Player), CCTags::Team::Player.GetTag());
	TestFalse(TEXT("同阵营不敌对"), UCC_TeamStatics::IsHostile(EnemyA, EnemyB));
	TestTrue(TEXT("敌人与玩家敌对"), UCC_TeamStatics::IsHostile(EnemyA, Player));
	TestTrue(TEXT("敌对关系对称"), UCC_TeamStatics::IsHostile(Player, EnemyA));
	TestFalse(TEXT("自己不与自己敌对"), UCC_TeamStatics::IsHostile(EnemyA, EnemyA));
	TestFalse(TEXT("空指针不敌对"), UCC_TeamStatics::IsHostile(EnemyA, nullptr));

	const FActiveGameplayEffectHandle Charm = ApplyCharm(EnemyB, Player);
	TestTrue(TEXT("魅惑效果已施加"), Charm.IsValid());
	TestTrue(TEXT("魅惑授予 Charmed 标签"), EnemyB->GetAbilitySystemComponent()->HasMatchingGameplayTag(CCTags::Status::Charmed));
	TestEqual(TEXT("被魅惑后有效阵营 = 魅惑者阵营"), UCC_TeamStatics::GetEffectiveTeam(EnemyB), CCTags::Team::Player.GetTag());
	TestEqual(TEXT("基础阵营不受魅惑影响"), UCC_TeamStatics::GetBaseTeam(EnemyB), CCTags::Team::Enemy.GetTag());
	TestTrue(TEXT("被魅惑的同伴变成敌人"), UCC_TeamStatics::IsHostile(EnemyA, EnemyB));
	TestFalse(TEXT("被魅惑者不再攻击魅惑者"), UCC_TeamStatics::IsHostile(EnemyB, Player));

	EnemyB->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(Charm);
	TestEqual(TEXT("解除魅惑后恢复基础阵营"), UCC_TeamStatics::GetEffectiveTeam(EnemyB), CCTags::Team::Enemy.GetTag());
	TestFalse(TEXT("解除魅惑后同伴恢复友方"), UCC_TeamStatics::IsHostile(EnemyA, EnemyB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCAITargetSelectionTest, "GAS_Demo.AI.TargetSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 验证 ACC_AIController::SelectBestTarget：过滤友方与死亡目标、取最近、迟滞、魅惑后的目标变化。
 * 布局（X 轴）：Self 0，友方 Ally 100，玩家 Near 500，玩家 Far 650。
 */
bool FCCAITargetSelectionTest::RunTest(const FString& Parameters)
{
	using namespace CCTeamTests;
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("World"), World)) return false;
	ON_SCOPE_EXIT { DestroyTestWorld(World); };

	auto* Self = SpawnAlive<ACC_EnemyCharacter>(World, FVector(0.f, 0.f, 0.f));
	auto* Ally = SpawnAlive<ACC_EnemyCharacter>(World, FVector(100.f, 0.f, 0.f));
	auto* Near = SpawnAlive<ACC_PlayerCharacter>(World, FVector(500.f, 0.f, 0.f));
	auto* Far = SpawnAlive<ACC_PlayerCharacter>(World, FVector(650.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Self"), Self) || !TestNotNull(TEXT("Ally"), Ally)
		|| !TestNotNull(TEXT("Near"), Near) || !TestNotNull(TEXT("Far"), Far))
		return false;

	auto Select = [Self](const TArray<AActor*>& Candidates, AActor* Current, float Hysteresis = 200.f)
	{
		return ACC_AIController::SelectBestTarget(Self, Candidates, Current, Hysteresis);
	};

	TestTrue(TEXT("友方不是目标，取最近的敌人"), Select({Ally, Far, Near}, nullptr) == Near);
	TestTrue(TEXT("只有友方时没有目标"), Select({Ally}, nullptr) == nullptr);
	TestTrue(TEXT("没有候选时没有目标"), Select({}, nullptr) == nullptr);
	TestTrue(TEXT("距离优势 150 < 迟滞 200：保持当前目标"), Select({Near, Far}, Far) == Far);
	TestTrue(TEXT("距离优势 150 > 迟滞 100：切换到更近的"), Select({Near, Far}, Far, 100.f) == Near);
	TestTrue(TEXT("其他目标离开视野不影响当前目标"), Select({Near}, Near) == Near);
	TestTrue(TEXT("当前目标离开视野时换到仍可见的目标"), Select({Far}, Near) == Far);

	Near->GetRuntimeData()->SetAlive(false);
	TestTrue(TEXT("当前目标死亡后换到其他目标"), Select({Near, Far}, Near) == Far);
	TestTrue(TEXT("唯一目标死亡后没有目标"), Select({Near}, Near) == nullptr);
	Near->GetRuntimeData()->SetAlive(true);

	// 自己被玩家魅惑：玩家变成友方，原来的同伴变成敌人
	const FActiveGameplayEffectHandle SelfCharm = ApplyCharm(Self, Near);
	TestTrue(TEXT("自己被魅惑后攻击原同伴"), Select({Ally, Near, Far}, Near) == Ally);
	Self->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(SelfCharm);
	TestTrue(TEXT("解除魅惑后恢复攻击玩家"), Select({Ally, Near, Far}, nullptr) == Near);

	// 同伴被玩家魅惑：它比玩家近得多，超过迟滞距离，切换过去
	const FActiveGameplayEffectHandle AllyCharm = ApplyCharm(Ally, Near);
	TestTrue(TEXT("被魅惑的同伴成为目标"), Select({Ally, Near}, Near) == Ally);
	Ally->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(AllyCharm);
	TestTrue(TEXT("同伴解除魅惑后不再是目标"), Select({Ally, Near}, Ally) == Near);
	return true;
}
#endif
