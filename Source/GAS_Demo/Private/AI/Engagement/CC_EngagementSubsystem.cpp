#include "AI/Engagement/CC_EngagementSubsystem.h"

#include "Actors/Character/CC_BaseCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GAS_Demo.h"
#include "HAL/IConsoleManager.h"
#include "Utils/Team/CC_TeamStatics.h"

namespace
{
	/** 兜底清理间隔（秒）。 */
	constexpr float PruneInterval = 0.5f;

	TAutoConsoleVariable<int32> CVarEngagementDebug(
		TEXT("cc.Engagement.Debug"), 0,
		TEXT("Draw attacker -> target lines (melee red, ranged cyan) and blocked attackers. 0 = off, 1 = on."),
		ECVF_Default);

	bool IsAliveActor(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}
		const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Actor);
		return !Character || Character->IsAlive();
	}
}

#pragma region 生命周期

bool UCC_EngagementSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// 只在游戏 World 中创建。不在这里判断 NetMode：子系统创建时客户端的网络驱动可能还没就绪；
	// 客户端没有 AIController，也就不会有任何登记，Tick 会立即返回。
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UCC_EngagementSubsystem::Deinitialize()
{
	Records.Empty();
	MeleeByTarget.Empty();
	Super::Deinitialize();
}

TStatId UCC_EngagementSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCC_EngagementSubsystem, STATGROUP_Tickables);
}

void UCC_EngagementSubsystem::Tick(float DeltaTime)
{
	if (Records.IsEmpty())
	{
		return;
	}
	TimeSincePrune += DeltaTime;
	if (TimeSincePrune >= PruneInterval)
	{
		TimeSincePrune = 0.f;
		PruneInvalid();
	}
	if (IsDebugDrawEnabled())
	{
		DrawDebug();
	}
}

bool UCC_EngagementSubsystem::IsDebugDrawEnabled()
{
	return CVarEngagementDebug.GetValueOnGameThread() > 0;
}

#pragma endregion

#pragma region 对外接口

void UCC_EngagementSubsystem::RegisterAttacker(AActor* Attacker, AActor* Target, bool bRanged)
{
	if (!IsValid(Attacker))
	{
		return;
	}
	if (!IsValid(Target) || Attacker == Target)
	{
		UnregisterAttacker(Attacker);
		return;
	}

	const FObjectKey AttackerKey(Attacker);
	const FObjectKey TargetKey(Target);
	if (FCC_EngagementRecord* Existing = Records.Find(AttackerKey))
	{
		if (Existing->TargetKey == TargetKey)
		{
			// 同一目标：只同步兵种（例如运行时换了配置），不打日志，控制器每 0.5 秒会自愈注册一次。
			if (Existing->bRanged != bRanged)
			{
				Existing->bRanged = bRanged;
				if (bRanged)
				{
					if (TSet<FObjectKey>* Melee = MeleeByTarget.Find(TargetKey))
					{
						Melee->Remove(AttackerKey);
					}
				}
				else
				{
					MeleeByTarget.FindOrAdd(TargetKey).Add(AttackerKey);
				}
			}
			return;
		}
		RemoveRecord(AttackerKey);
	}

	FCC_EngagementRecord& Record = Records.Add(AttackerKey);
	Record.Attacker = Attacker;
	Record.Target = Target;
	Record.TargetKey = TargetKey;
	Record.bRanged = bRanged;
	if (!bRanged)
	{
		MeleeByTarget.FindOrAdd(TargetKey).Add(AttackerKey);
	}
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Engagement] Register: Attacker=%s Target=%s Ranged=%d"),
		*GetNameSafe(Attacker), *GetNameSafe(Target), bRanged);
}

void UCC_EngagementSubsystem::UnregisterAttacker(AActor* Attacker)
{
	if (!Attacker)
	{
		return;
	}
	const FObjectKey AttackerKey(Attacker);
	if (Records.Contains(AttackerKey))
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Engagement] Unregister: Attacker=%s"), *GetNameSafe(Attacker));
		RemoveRecord(AttackerKey);
	}
}

int32 UCC_EngagementSubsystem::GetMeleeAttackerCount(const AActor* Target, const AActor* Exclude) const
{
	if (!Target)
	{
		return 0;
	}
	const TSet<FObjectKey>* Melee = MeleeByTarget.Find(FObjectKey(Target));
	if (!Melee)
	{
		return 0;
	}
	int32 Count = Melee->Num();
	if (Exclude && Melee->Contains(FObjectKey(Exclude)))
	{
		--Count;
	}
	return Count;
}

AActor* UCC_EngagementSubsystem::GetTargetOf(const AActor* Attacker) const
{
	if (!Attacker)
	{
		return nullptr;
	}
	const FCC_EngagementRecord* Record = Records.Find(FObjectKey(Attacker));
	return Record ? Record->Target.Get() : nullptr;
}

#pragma endregion

#pragma region 内部

void UCC_EngagementSubsystem::RemoveRecord(const FObjectKey& AttackerKey)
{
	const FCC_EngagementRecord* Record = Records.Find(AttackerKey);
	if (!Record)
	{
		return;
	}
	const FObjectKey TargetKey = Record->TargetKey;
	if (TSet<FObjectKey>* Melee = MeleeByTarget.Find(TargetKey))
	{
		Melee->Remove(AttackerKey);
		if (Melee->IsEmpty())
		{
			MeleeByTarget.Remove(TargetKey);
		}
	}
	Records.Remove(AttackerKey);
}

/**
 * 兜底清理：攻击者或目标已销毁 / 死亡，或两者已不再敌对（魅惑变化）。
 * 控制器的注销路径漏掉时，记录最多残留一个清理周期，只会让拥挤计数短暂偏大。
 */
void UCC_EngagementSubsystem::PruneInvalid()
{
	TArray<FObjectKey> ToRemove;
	for (const TPair<FObjectKey, FCC_EngagementRecord>& Pair : Records)
	{
		const AActor* Attacker = Pair.Value.Attacker.Get();
		const AActor* Target = Pair.Value.Target.Get();
		if (!IsAliveActor(Attacker) || !IsAliveActor(Target) || !UCC_TeamStatics::IsHostile(Attacker, Target))
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FObjectKey& Key : ToRemove)
	{
		const FCC_EngagementRecord& Record = Records.FindChecked(Key);
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Engagement] Pruned: Attacker=%s Target=%s"),
			*GetNameSafe(Record.Attacker.Get()), *GetNameSafe(Record.Target.Get()));
		RemoveRecord(Key);
	}
}

void UCC_EngagementSubsystem::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// 只画一帧，每帧重画：近战红线、弓兵青线；目标头顶显示近战人数。
	for (const TPair<FObjectKey, FCC_EngagementRecord>& Pair : Records)
	{
		const AActor* Attacker = Pair.Value.Attacker.Get();
		const AActor* Target = Pair.Value.Target.Get();
		if (!Attacker || !Target)
		{
			continue;
		}
		DrawDebugLine(World, Attacker->GetActorLocation(), Target->GetActorLocation(),
			Pair.Value.bRanged ? FColor::Cyan : FColor::Red, false, -1.f, 0, 1.5f);
	}
	for (const TPair<FObjectKey, TSet<FObjectKey>>& Pair : MeleeByTarget)
	{
		const AActor* Target = Cast<AActor>(Pair.Key.ResolveObjectPtr());
		if (Target)
		{
			DrawDebugString(World, Target->GetActorLocation() + FVector(0.f, 0.f, 140.f),
				FString::Printf(TEXT("Melee %d"), Pair.Value.Num()), nullptr, FColor::White, 0.f);
		}
	}
#endif
}

#pragma endregion
