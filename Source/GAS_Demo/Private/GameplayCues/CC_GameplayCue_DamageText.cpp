#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace CCDamageCueTags
{
	UE_DEFINE_GAMEPLAY_TAG(DamageText, "GameplayCue.Combat.DamageText");
	UE_DEFINE_GAMEPLAY_TAG(ManualDisplay, "GameplayCue.DamageText.ManualDisplay");
}

/**
 * 设置独立伤害数字 Cue 标签，与声音和 Niagara 特效分离。
 */
UCC_GameplayCue_DamageText::UCC_GameplayCue_DamageText()
{
	GameplayCueTag = CCDamageCueTags::DamageText;
	GameplayCueName = GameplayCueTag.GetTagName();
}

/**
 * 只响应单次 Executed 事件。
 * @param Type 本次 GameplayCue 事件类型。
 * @return 事件为 Executed 时返回 true，其他事件返回 false。
 */
bool UCC_GameplayCue_DamageText::HandlesEvent(EGameplayCueEvent::Type Type) const
{
	return Type == EGameplayCueEvent::Executed;
}

/**
 * 解析伤害/治疗样式，筛选本地观看者，并提交独立飘字条目。
 * @param Target 接收数字 Cue 的目标 Actor。
 * @param Params 数字 Cue 参数：位置、来源、数值以及扩展 Context。
 * @return 至少向一个本地玩家提交数字时返回 true。
 */
bool UCC_GameplayCue_DamageText::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Params) const
{
	UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || !FMath::IsFinite(Params.RawMagnitude) ||
		FMath::IsNearlyZero(Params.RawMagnitude)) return false;
	auto* Subsystem = World->GetSubsystem<UCC_DamageTextSubsystem>();
	if (!Subsystem) return false;
	const auto* Context = FCC_GameplayEffectContext::Get(Params.EffectContext);
	const bool bHeal = Params.RawMagnitude < 0.f;
	const auto Style = bHeal
		                   ? ECC_DamageTextStyle::Heal
		                   : (Context && Context->bCritical
			                      ? ECC_DamageTextStyle::Critical
			                      : ECC_DamageTextStyle::Normal);
	bool bSubmitted = false;
	// 分屏逐玩家筛选：默认显示自己的攻击和自己受到的伤害；开关可允许显示全部。
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player || !Player->IsLocalController()) continue;
		auto IsOwnedByPlayer = [Player](AActor* Actor)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			return Actor && (Actor == Player || Actor == Player->GetPawn() || Actor->IsOwnedBy(Player)
				|| (Pawn && Pawn->GetController() == Player));
		};
		const bool bManualDisplay = Params.AggregatedSourceTags.HasTagExact(CCDamageCueTags::ManualDisplay);
		if (!bManualDisplay && !Subsystem->bShowAllDamage && !IsOwnedByPlayer(Params.Instigator.Get()) && !
			IsOwnedByPlayer(Target)) continue;
		// 保存来源/目标/玩家身份，后续合并不能再靠位置猜测是不是同一个敌人。
		Subsystem->ReportHitForPlayer(Player, Target, Params.Instigator.Get(), Params.Location, Params.RawMagnitude,
		                              Style, Context ? Context->DamageType : FGameplayTag());
		bSubmitted = true;
	}
	return bSubmitted;
}
