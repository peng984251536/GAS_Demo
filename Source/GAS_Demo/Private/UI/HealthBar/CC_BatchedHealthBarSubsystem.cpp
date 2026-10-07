#include "UI/HealthBar/CC_BatchedHealthBarSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// 通过控制器寻找本地玩家子系统；专用服务器及远程控制器没有这份本地 UI 数据。
UCC_BatchedHealthBarSubsystem* UCC_BatchedHealthBarSubsystem::GetHealthBarManager(APlayerController* PC)
{
	return IsValid(PC) && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UCC_BatchedHealthBarSubsystem>() : nullptr;
}

// 条目以 Actor 身份去重，避免同一角色注册出多条血条。
FCC_HealthBarEntry* UCC_BatchedHealthBarSubsystem::Find(AActor* Actor)
{
	return Entries.FindByPredicate([Actor](const FCC_HealthBarEntry& Entry) { return Entry.Actor.Get() == Actor; });
}

// 仅登记数据，不给角色挂载组件，也不创建 HUD。GAS 未就绪时保留条目等待后续重试。
bool UCC_BatchedHealthBarSubsystem::RegisterHealthBar(AActor* Actor, const FCC_HealthBarOptions& Options)
{
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld() || !GetWorld() || !GetWorld()->IsGameWorld()) return false;
	if (!FMath::IsFinite(Options.Size.X) || !FMath::IsFinite(Options.Size.Y) ||
		Options.Size.X <= 0 || Options.Size.Y <= 0 || Options.WorldOffset.ContainsNaN() ||
		!FMath::IsFinite(Options.MaxDistance)) return false;
	FCC_HealthBarEntry* Entry = Find(Actor);
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Actor = Actor;
	}
	// 重复注册先清理旧监听。手动模式会重置就绪状态，需要蓝图重新推送一次初值。
	Unbind(*Entry);
	Entry->Options = Options;
	Entry->Options.MaxDistance = FMath::Max(0.f, Options.MaxDistance);
	Entry->bReady = false;
	if (Options.bUseGAS) Bind(*Entry);
	OnEntriesUpdated.Broadcast();
	return true;
}

// 只移除当前条目保存的监听句柄，保证重复注册、替换 ASC 和移除时不会重复订阅。
void UCC_BatchedHealthBarSubsystem::Unbind(FCC_HealthBarEntry& Entry)
{
	if (UAbilitySystemComponent* ASC = Entry.ASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute()).Remove(Entry.HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMaxHealthAttribute()).Remove(Entry.MaxHealthHandle);
	}
	Entry.ASC.Reset();
	Entry.HealthHandle.Reset();
	Entry.MaxHealthHandle.Reset();
}

// 一次读取当前值和最大值，保证任一属性变化后比例使用同一数据源的最新数值。
void UCC_BatchedHealthBarSubsystem::ReadVitals(FCC_HealthBarEntry& Entry)
{
	UAbilitySystemComponent* ASC = Entry.ASC.Get();
	if (!ASC) return;
	Entry.Health = ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute());
	Entry.MaxHealth = ASC->GetNumericAttribute(UCC_AttributeSet::GetMaxHealthAttribute());
	if (!FMath::IsFinite(Entry.Health) || !FMath::IsFinite(Entry.MaxHealth))
	{
		Entry.bReady = false;
		return;
	}
	// 初次显示直接定位到当前比例；后续属性变化保留旧显示比例，由 Tick 平滑靠近新值。
	if (!Entry.bReady) Entry.DisplayedFraction = Entry.MaxHealth > 0 ? FMath::Clamp(Entry.Health / Entry.MaxHealth, 0.f, 1.f) : 0.f;
	Entry.bReady = true;
}

// 每次低频检查都重新解析角色的数据源；源未改变时立即返回，避免重复绑定和属性轮询。
void UCC_BatchedHealthBarSubsystem::Bind(FCC_HealthBarEntry& Entry)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Entry.Actor.Get());
	const bool bHasVitals = ASC && ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute()) &&
		ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetMaxHealthAttribute());
	// ASC 或属性集可能比角色晚创建；此时先停止显示，下一次绑定检查再尝试。
	if (!bHasVitals)
	{
		Unbind(Entry);
		Entry.bReady = false;
		return;
	}
	if (Entry.ASC.Get() == ASC) return;
	Unbind(Entry);
	Entry.bReady = false;
	Entry.ASC = ASC;
	// 不捕获 Entry 的地址：数组扩容或 RemoveAtSwap 会移动元素。回调使用弱 Actor 引用重新查找当前条目。
	const TWeakObjectPtr<AActor> WeakActor = Entry.Actor;
	auto OnChanged = [this, WeakActor](const FOnAttributeChangeData&)
	{
		if (WeakActor.IsValid()) if (FCC_HealthBarEntry* Current = Find(WeakActor.Get())) ReadVitals(*Current);
	};
	Entry.HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute()).AddWeakLambda(this, OnChanged);
	Entry.MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMaxHealthAttribute()).AddWeakLambda(this, OnChanged);
	// 委托只处理未来变化，必须主动补推一次当前值，避免初始血条一直未就绪。
	ReadVitals(Entry);
}

// 手动模式和 GAS 模式互斥，防止蓝图推送覆盖 GAS 的权威属性显示。
bool UCC_BatchedHealthBarSubsystem::UpdateHealthBar(AActor* Actor, float Health, float MaxHealth)
{
	if (!IsValid(Actor) || !FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth)) return false;
	FCC_HealthBarEntry* Entry = Find(Actor);
	if (!Entry || Entry->Options.bUseGAS) return false;
	Entry->Health = Health;
	Entry->MaxHealth = FMath::Max(0.f, MaxHealth);
	if (!Entry->bReady) Entry->DisplayedFraction = Entry->MaxHealth > 0 ? FMath::Clamp(Health / Entry->MaxHealth, 0.f, 1.f) : 0.f;
	Entry->bReady = true;
	OnEntriesUpdated.Broadcast();
	return true;
}

// 先解绑，再倒序移除；不依赖角色是否已显示，也不保留对角色的强引用。
void UCC_BatchedHealthBarSubsystem::UnregisterHealthBar(AActor* Actor)
{
	if (!Actor) return;
	bool bRemoved = false;
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
		if (Entries[Index].Actor == Actor)
		{
			Unbind(Entries[Index]);
			Entries.RemoveAtSwap(Index);
			bRemoved = true;
		}
	// 移除最后一个条目后子系统停止 Tick，必须在这里通知，避免界面保留旧快照。
	if (bRemoved) OnEntriesUpdated.Broadcast();
}

// 显隐只修改显示标记；恢复显示时可以立即使用一直在更新的血量。
void UCC_BatchedHealthBarSubsystem::SetHealthBarVisible(AActor* Actor, bool bVisible)
{
	if (IsValid(Actor)) if (FCC_HealthBarEntry* Entry = Find(Actor))
	{
		Entry->bVisible = bVisible;
		OnEntriesUpdated.Broadcast();
	}
}

// 释放全部监听后清空列表，下次注册从初始绑定检查开始。
void UCC_BatchedHealthBarSubsystem::ClearHealthBars()
{
	for (FCC_HealthBarEntry& Entry : Entries) Unbind(Entry);
	Entries.Reset();
	BindingTimer = 0;
	OnEntriesUpdated.Broadcast();
}

// 子系统结束前主动解除监听，避免存活的 ASC 留下不再使用的订阅。
void UCC_BatchedHealthBarSubsystem::Deinitialize()
{
	ClearHealthBars();
	Super::Deinitialize();
}

// 只在有条目且位于游戏世界时工作；CDO 与编辑器预览对象不参与 Tick。
bool UCC_BatchedHealthBarSubsystem::IsTickable() const
{
	return !IsTemplate() && Entries.Num() > 0 && GetWorld() && GetWorld()->IsGameWorld();
}

// 为集中管理的 Tick 提供独立统计标识。
TStatId UCC_BatchedHealthBarSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCC_BatchedHealthBarSubsystem, STATGROUP_Tickables);
}

// 每帧只负责生命周期和插值，GAS 数据源每 0.25 秒检查一次，数值变化通过事件推送。
void UCC_BatchedHealthBarSubsystem::Tick(float DeltaTime)
{
	BindingTimer -= DeltaTime;
	const bool bCheckBindings = BindingTimer <= 0;
	if (bCheckBindings) BindingTimer = 0.25f;
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FCC_HealthBarEntry& Entry = Entries[Index];
		AActor* Actor = Entry.Actor.Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
		{
			Unbind(Entry);
			Entries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (bCheckBindings && Entry.Options.bUseGAS) Bind(Entry);
		// 最大值为 0 时使用空比例，防止除零；插值仅影响表现，不修改 GAS 属性。
		const float Fraction = Entry.MaxHealth > 0 ? FMath::Clamp(Entry.Health / Entry.MaxHealth, 0.f, 1.f) : 0.f;
		Entry.DisplayedFraction = FMath::FInterpTo(Entry.DisplayedFraction, Fraction, DeltaTime, 12.f);
	}
	// 本帧移除了最后一个条目时也会广播一次空列表；之后子系统停止 Tick。
	OnEntriesUpdated.Broadcast();
}
