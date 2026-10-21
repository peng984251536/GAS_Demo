#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/Engagement/Slots/CC_SlotRing.h"
#include "AI/Engagement/Tokens/CC_AttackTokenPool.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

/**
 * 槽位环的纯逻辑测试：槽位数、增量分配的保留 / 补位 / 夹角重分配 / 降档、不可用槽位。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCEngagementSlotRingTest, "GAS_Demo.AI.Engagement.SlotRing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCEngagementSlotRingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("内圈 180/120 → 9 个槽位"), FCC_SlotRing::ComputeSlotCount(180.f, 120.f), 9);
	TestEqual(TEXT("外圈 450/120 → 23 个槽位"), FCC_SlotRing::ComputeSlotCount(450.f, 120.f), 23);
	TestEqual(TEXT("无效参数至少 1 个"), FCC_SlotRing::ComputeSlotCount(0.f, 120.f), 1);

	const FVector Center = FVector::ZeroVector;
	FCC_SlotRing Ring;
	TestTrue(TEXT("第一次 Rebuild 生成槽位"), Ring.Rebuild(Center, 450.f, 120.f, 0.f, 0.f));
	TestEqual(TEXT("槽位数"), Ring.Slots.Num(), 23);
	TestFalse(TEXT("参数不变不重新生成"), Ring.Rebuild(Center + FVector(10.f, 0.f, 0.f), 450.f, 120.f, 0.f, 0.f));
	TestEqual(TEXT("只平移"), Ring.Slots[0].WorldLocation, Center + FVector(10.f, 0.f, 0.f) + Ring.Slots[0].LocalOffset);
	Ring.Rebuild(Center, 450.f, 120.f, 0.f, 0.f);

	// 两个攻击者分别在 +X 和 +Y 方向，应各自分到同侧的不同槽位。
	TArray<FVector> Positions = {FVector(600.f, 0.f, 0.f), FVector(0.f, 600.f, 0.f)};
	TArray<int32> Indices = {INDEX_NONE, INDEX_NONE};
	Ring.AssignIncremental(Center, Positions, Indices);
	TestTrue(TEXT("攻击者 0 分到槽位"), Indices[0] != INDEX_NONE);
	TestTrue(TEXT("攻击者 1 分到槽位"), Indices[1] != INDEX_NONE);
	TestNotEqual(TEXT("槽位不重复"), Indices[0], Indices[1]);
	TestTrue(TEXT("攻击者 0 的槽位在 +X 一侧"), Ring.Slots[Indices[0]].LocalOffset.X > 0.f);
	TestTrue(TEXT("攻击者 1 的槽位在 +Y 一侧"), Ring.Slots[Indices[1]].LocalOffset.Y > 0.f);

	// 再分配一次：旧槽位仍然合适，原样保留。
	const TArray<int32> Previous = Indices;
	Ring.AssignIncremental(Center, Positions, Indices);
	TestEqual(TEXT("保留攻击者 0 的旧槽位"), Indices[0], Previous[0]);
	TestEqual(TEXT("保留攻击者 1 的旧槽位"), Indices[1], Previous[1]);

	// 攻击者 0 跑到目标另一侧：夹角超过 90°，重新分配到 -X 一侧。
	Positions[0] = FVector(-600.f, 0.f, 0.f);
	Ring.AssignIncremental(Center, Positions, Indices);
	TestTrue(TEXT("夹角超过 90° 重新分配"), Indices[0] != Previous[0]);
	TestTrue(TEXT("新槽位在 -X 一侧"), Ring.Slots[Indices[0]].LocalOffset.X < 0.f);
	TestEqual(TEXT("攻击者 1 不受影响"), Indices[1], Previous[1]);

	// 攻击者 1 的槽位变得不可用（例如投影失败或被墙挡住）：重新分配。
	const int32 Blocked = Indices[1];
	Ring.Slots[Blocked].bUsable = false;
	Ring.AssignIncremental(Center, Positions, Indices);
	TestTrue(TEXT("不可用的槽位被换掉"), Indices[1] != INDEX_NONE && Indices[1] != Blocked);

	// 冷却中的槽位在 Rebuild 后仍不可用。
	Ring.Slots[0].CooldownUntil = 10.f;
	Ring.Rebuild(Center, 450.f, 120.f, 0.f, 5.f);
	TestFalse(TEXT("冷却中的槽位不可用"), Ring.Slots[0].bUsable);
	Ring.Rebuild(Center, 450.f, 120.f, 0.f, 11.f);
	TestTrue(TEXT("冷却结束恢复可用"), Ring.Slots[0].bUsable);

	// 攻击者多于槽位：离中心最远的人分不到。
	FCC_SlotRing Inner;
	Inner.Rebuild(Center, 180.f, 120.f, 0.f, 0.f);
	TArray<FVector> Crowd;
	TArray<int32> CrowdIndices;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const float Angle = 2.f * UE_PI * Index / 12;
		const float Distance = 300.f + Index * 10.f;
		Crowd.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance);
		CrowdIndices.Add(INDEX_NONE);
	}
	Inner.AssignIncremental(Center, Crowd, CrowdIndices);
	int32 Assigned = 0;
	for (int32 Index = 0; Index < CrowdIndices.Num(); ++Index)
	{
		Assigned += CrowdIndices[Index] != INDEX_NONE ? 1 : 0;
	}
	TestEqual(TEXT("只分配 9 个"), Assigned, 9);
	TestEqual(TEXT("最远的人降档"), CrowdIndices[11], INDEX_NONE);
	TestTrue(TEXT("最近的人有槽位"), CrowdIndices[0] != INDEX_NONE);
	TestEqual(TEXT("没有剩余空位"), Inner.CountFreeUsable(), 0);
	return true;
}

/**
 * 令牌池的纯逻辑测试：容量、幂等、归还、消耗值、清理。
 * 键使用几个已存在的 UClass 对象，不需要创建 World。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCEngagementTokenPoolTest, "GAS_Demo.AI.Engagement.TokenPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCEngagementTokenPoolTest::RunTest(const FString& Parameters)
{
	const FObjectKey A(UObject::StaticClass());
	const FObjectKey B(UClass::StaticClass());
	const FObjectKey C(UPackage::StaticClass());

	FCC_AttackTokenPool Pool;
	Pool.Capacity = 2;
	TestTrue(TEXT("A 申请成功"), Pool.TryAcquire(A));
	TestTrue(TEXT("A 重复申请幂等"), Pool.TryAcquire(A));
	TestEqual(TEXT("重复申请不多占"), Pool.Used(), 1);
	TestTrue(TEXT("B 申请成功"), Pool.TryAcquire(B));
	TestFalse(TEXT("容量已满，C 被拒"), Pool.TryAcquire(C));
	TestEqual(TEXT("余量为 0"), Pool.Available(), 0);

	TestTrue(TEXT("A 归还成功"), Pool.Release(A));
	TestFalse(TEXT("重复归还无效果"), Pool.Release(A));
	TestTrue(TEXT("归还后 C 可以申请"), Pool.TryAcquire(C));

	Pool.PruneIf([&B](const FObjectKey& Key) { return Key == B; });
	TestFalse(TEXT("B 被清理"), Pool.Holds(B));
	TestTrue(TEXT("C 仍持有"), Pool.Holds(C));

	Pool.Reset();
	TestFalse(TEXT("消耗 3 超过容量 2"), Pool.TryAcquire(A, 3));
	TestTrue(TEXT("消耗 2 刚好占满"), Pool.TryAcquire(A, 2));
	TestFalse(TEXT("占满后再申请失败"), Pool.TryAcquire(B));
	return true;
}

#endif
