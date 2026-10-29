#include "Game/Survival/CC_SurvivalModeConfig.h"

#include "Actors/Character/CC_EnemyCharacter.h"
#include "Data/Buff/CC_BuffOfferPool.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FCC_WaveTier* UCC_SurvivalModeConfig::FindTier(const FCC_WaveSettings& Settings, int32 Wave)
{
	const FCC_WaveTier* Found = nullptr;
	for (const FCC_WaveTier& Tier : Settings.Tiers)
	{
		if (Tier.StartWave <= Wave)
		{
			Found = &Tier;
		}
		else
		{
			break; // 按 StartWave 升序（IsDataValid 保证）
		}
	}
	return Found ? Found : (Settings.Tiers.IsEmpty() ? nullptr : &Settings.Tiers[0]);
}

int32 UCC_SurvivalModeConfig::ComputeCount(const FCC_WaveTier& Tier, int32 Wave, float CountScale)
{
	const int32 WavesIntoTier = FMath::Max(0, Wave - Tier.StartWave);
	const float Raw = (Tier.BaseCount + Tier.CountPerWave * WavesIntoTier) * FMath::Max(0.f, CountScale);
	return FMath::Max(0, FMath::RoundToInt(Raw));
}

float UCC_SurvivalModeConfig::EvaluateMultiplier(const FRuntimeFloatCurve& Curve, float X)
{
	const FRichCurve* Rich = Curve.GetRichCurveConst();
	if (!Rich || Rich->GetNumKeys() == 0)
	{
		return 1.f;
	}
	return FMath::Max(0.01f, Rich->Eval(X));
}

float UCC_SurvivalModeConfig::ComputeExperienceToNext(const FCC_ProgressionSettings& Settings, int32 Level)
{
	const int32 SafeLevel = FMath::Max(1, Level);
	const FRichCurve* Rich = Settings.ExperienceToNext.GetRichCurveConst();
	const float Value = (Rich && Rich->GetNumKeys() > 0)
		? Rich->Eval(static_cast<float>(SafeLevel))
		: Settings.BaseExperienceToNext + Settings.ExperienceToNextPerLevel * (SafeLevel - 1);
	return FMath::Max(1.f, Value);
}

int32 UCC_SurvivalModeConfig::ApplyExperience(const FCC_ProgressionSettings& Settings, int32& InOutLevel,
	float& InOutExperience, float Amount)
{
	InOutLevel = FMath::Max(1, InOutLevel);
	if (!FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return 0;
	}
	if (Settings.MaxLevel > 0 && InOutLevel >= Settings.MaxLevel)
	{
		return 0; // 满级后不再积累
	}
	InOutExperience += Amount;
	int32 LevelsGained = 0;
	// 上限保护：异常大的经验也不会让循环失控。
	for (int32 Guard = 0; Guard < 1000; ++Guard)
	{
		if (Settings.MaxLevel > 0 && InOutLevel >= Settings.MaxLevel)
		{
			InOutExperience = 0.f;
			break;
		}
		const float Needed = ComputeExperienceToNext(Settings, InOutLevel);
		if (InOutExperience < Needed)
		{
			break;
		}
		InOutExperience -= Needed;
		++InOutLevel;
		++LevelsGained;
	}
	return LevelsGained;
}

float UCC_SurvivalModeConfig::ComputeRespawnDelay(const FCC_RespawnSettings& Settings, int32 DeathCount)
{
	const float Delay = Settings.BaseDelay + Settings.DelayPerDeath * FMath::Max(0, DeathCount - 1);
	return FMath::Clamp(Delay, 0.f, FMath::Max(Settings.BaseDelay, Settings.MaxDelay));
}

#if WITH_EDITOR
EDataValidationResult UCC_SurvivalModeConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const auto Error = [&](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (Waves.Tiers.IsEmpty())
	{
		Error(NSLOCTEXT("CCSurvival", "NoTiers", "Waves.Tiers 至少需要一个档位。"));
	}
	for (int32 Index = 0; Index < Waves.Tiers.Num(); ++Index)
	{
		const FCC_WaveTier& Tier = Waves.Tiers[Index];
		if (Index == 0 && Tier.StartWave != 1)
		{
			Error(NSLOCTEXT("CCSurvival", "FirstTier", "第一个档位的 StartWave 必须为 1。"));
		}
		if (Index > 0 && Tier.StartWave <= Waves.Tiers[Index - 1].StartWave)
		{
			Error(FText::Format(NSLOCTEXT("CCSurvival", "TierOrder", "Tiers[{0}].StartWave 必须大于上一档。"), FText::AsNumber(Index)));
		}
		if (Tier.Enemies.IsEmpty())
		{
			Error(FText::Format(NSLOCTEXT("CCSurvival", "TierEnemies", "Tiers[{0}] 没有配置怪物。"), FText::AsNumber(Index)));
		}
		for (const FCC_WeightedEnemy& Enemy : Tier.Enemies)
		{
			if (Enemy.EnemyClass.IsNull())
			{
				Error(FText::Format(NSLOCTEXT("CCSurvival", "TierEnemyNull", "Tiers[{0}] 有空的怪物类。"), FText::AsNumber(Index)));
			}
		}
	}

	if (!Progression.BuffPool)
	{
		Context.AddWarning(NSLOCTEXT("CCSurvival", "NoPool", "Progression.BuffPool 为空：升级不会弹出三选一。"));
	}
	else
	{
		for (const FCC_BuffOfferOption& Option : Progression.BuffPool->Entries)
		{
			const UGameplayEffect* Effect = Option.Effect ? Option.Effect->GetDefaultObject<UGameplayEffect>() : nullptr;
			const UCC_BuffRulesComponent* Rules = Effect ? Effect->FindComponent<UCC_BuffRulesComponent>() : nullptr;
			// 生命周期设置检查，不是针对具体 Buff 的玩法判断。
			if (Effect && (!Rules || Rules->bRemoveOnDeath))
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("CCSurvival", "RemoveOnDeath",
					"{0}：升级 Buff 应在 CC Buff Rules 中关闭 bRemoveOnDeath，否则玩家阵亡后会丢失。"),
					FText::FromString(GetNameSafe(Option.Effect.Get()))));
			}
		}
	}

	// 人数缩放曲线在 1 人时通常应为 1；常见错误是引用了别的用途的外部曲线（例如按波次的曲线）。
	const auto CheckOnePlayer = [&](const FRuntimeFloatCurve& Curve, const TCHAR* Name)
	{
		const float AtOne = EvaluateMultiplier(Curve, 1.f);
		if (!FMath::IsNearlyEqual(AtOne, 1.f, 0.05f))
		{
			Context.AddWarning(FText::Format(NSLOCTEXT("CCSurvival", "ScaleAtOne",
				"PlayerScaling.{0} 在 1 名玩家时为 {1}（通常应为 1）。检查是否误引用了外部曲线。"),
				FText::FromString(Name), FText::AsNumber(AtOne)));
		}
	};
	CheckOnePlayer(PlayerScaling.CountMultiplier, TEXT("CountMultiplier"));
	CheckOnePlayer(PlayerScaling.HealthMultiplier, TEXT("HealthMultiplier"));

	if (Respawn.MaxRadius < Respawn.MinRadius)
	{
		Error(NSLOCTEXT("CCSurvival", "RespawnRadius", "Respawn.MaxRadius 不能小于 MinRadius。"));
	}
	return Result;
}
#endif
