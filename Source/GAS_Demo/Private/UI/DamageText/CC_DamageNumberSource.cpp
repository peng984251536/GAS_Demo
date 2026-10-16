#include "UI/DamageText/CC_DamageNumberSource.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSettings.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** 伤害数字压在血条等其他指示器上面。 */
	constexpr int32 DamageNumberPriority = 100;
}

UCC_DamageNumberSource* UCC_DamageNumberSource::GetForPlayer(const APlayerController* PlayerController)
{
	const ULocalPlayer* Player = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
	return Player ? Player->GetSubsystem<UCC_DamageNumberSource>() : nullptr;
}

void UCC_DamageNumberSource::Initialize(FSubsystemCollectionBase& Collection)
{
	UCC_WorldIndicatorSubsystem* Indicators = Collection.InitializeDependency<UCC_WorldIndicatorSubsystem>();
	Super::Initialize(Collection);
	if (Indicators) RemovedHandle = Indicators->OnIndicatorRemoved.AddUObject(this, &ThisClass::HandleIndicatorRemoved);
}

void UCC_DamageNumberSource::Deinitialize()
{
	if (const ULocalPlayer* Player = GetLocalPlayer())
		if (UCC_WorldIndicatorSubsystem* Indicators = Player->GetSubsystem<UCC_WorldIndicatorSubsystem>())
			Indicators->OnIndicatorRemoved.Remove(RemovedHandle);
	Active.Reset();
	Free.Reset();
	Super::Deinitialize();
}

void UCC_DamageNumberSource::ShowNumber(AActor* Target, const FVector& Location, float Amount, ECC_DamageTextStyle Style,
	FGameplayTag DamageType)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	UCC_WorldIndicatorSubsystem* Indicators = Player ? Player->GetSubsystem<UCC_WorldIndicatorSubsystem>() : nullptr;
	Amount = FMath::Abs(Amount);
	if (!Indicators || !FMath::IsFinite(Amount) || FMath::IsNearlyZero(Amount) || Location.ContainsNaN()) return;
	const UCC_WorldIndicatorSettings* Settings = GetDefault<UCC_WorldIndicatorSettings>();

	// 数量上限：先淘汰最旧的普通数字；全是重要数字时丢弃新的普通数字。
	if (Active.Num() >= FMath::Max(1, Settings->MaxDamageNumbersPerPlayer))
	{
		const int32 OldestNormal = Active.IndexOfByPredicate([](const UCC_DamageNumberIndicator* Number)
		{
			return Number->Style == ECC_DamageTextStyle::Normal;
		});
		if (OldestNormal == INDEX_NONE && Style == ECC_DamageTextStyle::Normal) return;
		Indicators->RemoveIndicator(Active[OldestNormal != INDEX_NONE ? OldestNormal : 0]); // 回收在 HandleIndicatorRemoved。
	}

	// 同一目标已有的数字越多，新数字越往两侧错开：0, +1, -1, +2, -2 …（最多 7 个槽位循环）。
	int32 Slot = 0;
	for (const UCC_DamageNumberIndicator* Number : Active) if (Number->Target == Target) ++Slot;
	Slot %= 7;
	const int32 Ring = (Slot + 1) / 2;

	UCC_DamageNumberIndicator* Number = Free.IsEmpty() ? NewObject<UCC_DamageNumberIndicator>(this) : Free.Pop(EAllowShrinking::No).Get();
	Number->SetAnchorActor(nullptr);
	Number->WorldLocation = Location;
	Number->WorldOffset = FVector::ZeroVector;
	Number->ScreenOffset = FVector2D((Slot % 2 ? 1.f : -1.f) * Ring * 18.f, -Ring * 6.f);
	Number->Alignment = FVector2D(0.5f, 0.5f);
	Number->MaxDistance = 0.f;
	Number->Priority = DamageNumberPriority;
	Number->bVisible = true;
	Number->WidgetClass = Settings->DamageNumberWidgetClass;
	Number->Amount = Amount;
	Number->Style = Style;
	Number->DamageType = DamageType;
	Number->Target = Target;
	Active.Add(Number);
	Indicators->AddIndicator(Number);
}

// 数字控件播完、超出上限或切图清空时都会走到这里，统一回收。
void UCC_DamageNumberSource::HandleIndicatorRemoved(UCC_WorldIndicator* Indicator)
{
	UCC_DamageNumberIndicator* Number = Cast<UCC_DamageNumberIndicator>(Indicator);
	if (!Number || Active.RemoveSingle(Number) == 0) return;
	Number->Target.Reset();
	Free.Add(Number);
}
