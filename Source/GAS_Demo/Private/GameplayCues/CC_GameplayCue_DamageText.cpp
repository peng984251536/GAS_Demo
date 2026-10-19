#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "Damage/CC_DamageTags.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "UI/DamageText/CC_DamageNumberSource.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace CCDamageCueTags
{
	UE_DEFINE_GAMEPLAY_TAG(ManualDisplay, "GameplayCue.DamageText.ManualDisplay");
}

namespace
{
	/** 命中点优先，否则用目标包围盒顶部。 */
	FVector ResolveTextLocation(const AActor* Target, const FGameplayCueParameters& Params, float HeadOffset)
	{
		if (const FHitResult* Hit = Params.EffectContext.GetHitResult()) return Hit->ImpactPoint;
		FVector Origin, Extent;
		Target->GetActorBounds(true, Origin, Extent);
		return Origin + FVector(0.f, 0.f, Extent.Z + HeadOffset);
	}

	/** Spec 标签里第一个 Damage.Type.* 子标签。 */
	FGameplayTag FindDamageType(const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
			if (Tag != CCDamageTags::TypeRoot && Tag.MatchesTag(CCDamageTags::TypeRoot)) return Tag;
		return FGameplayTag();
	}

	bool IsOwnedByPlayer(const AActor* Actor, const APlayerController* Player)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		return Actor && (Actor == Player || Actor == Player->GetPawn() || Actor->IsOwnedBy(Player)
			|| (Pawn && Pawn->GetController() == Player));
	}
}

UCC_GameplayCue_DamageText::UCC_GameplayCue_DamageText()
{
	GameplayCueTag = CCDamageTags::CueDamageText;
	GameplayCueName = GameplayCueTag.GetTagName();
}

UCC_GameplayCue_HealText::UCC_GameplayCue_HealText()
{
	GameplayCueTag = CCDamageTags::CueHealText;
	GameplayCueName = GameplayCueTag.GetTagName();
	bHealing = true;
}

bool UCC_GameplayCue_DamageText::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Params) const
{
	UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
	const float Amount = FMath::Abs(Params.RawMagnitude);
	if (!World || World->GetNetMode() == NM_DedicatedServer || !FMath::IsFinite(Amount) || FMath::IsNearlyZero(Amount)) return false;
	const FCC_GameplayEffectContext* Context = FCC_GameplayEffectContext::Get(Params.EffectContext);
	const ECC_DamageTextStyle Style = bHealing ? ECC_DamageTextStyle::Heal
		: (Context && Context->bCritical ? ECC_DamageTextStyle::Critical : ECC_DamageTextStyle::Normal);
	const FVector Location = ResolveTextLocation(Target, Params, HeadOffset);
	const FGameplayTag DamageType = FindDamageType(Params.AggregatedSourceTags);
	AActor* Instigator = Params.GetInstigator();
	const bool bManualDisplay = Params.AggregatedSourceTags.HasTagExact(CCDamageCueTags::ManualDisplay);

	const bool bShowAll = GetDefault<UCC_WorldIndicatorSettings>()->bShowAllDamageNumbers;

	// 分屏逐玩家筛选：默认只显示自己造成的和自己受到的；项目设置可改为显示全部。
	bool bSubmitted = false;
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player || !Player->IsLocalController()) continue;
		if (!bManualDisplay &&
			!bShowAll &&
			!IsOwnedByPlayer(Instigator, Player) &&
			!IsOwnedByPlayer(Target, Player))
			continue;
		//为数字创建一个固定坐标的指示器；
		//数字控件播完动画后自己移除，
		//这里回收指示器对象复用
		// 相当于是对象池管理器
		UCC_DamageNumberSource* Numbers = UCC_DamageNumberSource::GetForPlayer(Player);
		if (!Numbers) continue;
		Numbers->ShowNumber(Target, Location, Amount, Style, DamageType);
		bSubmitted = true;
	}
	return bSubmitted;
}
