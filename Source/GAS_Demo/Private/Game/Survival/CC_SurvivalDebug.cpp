// 生存模式调试：调参 CVar 与控制台命令。命令只在服务器（单机 / 监听服务器主机）上生效。
#include "Game/Survival/CC_SurvivalCVars.h"

#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameMode.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "GAS_Demo.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
namespace
{
	float GWaveIntervalScale = 1.f;
	float GSpawnCountScale = 1.f;
	float GExperienceScale = 1.f;
	int32 GSurvivalDebug = 0;

	FAutoConsoleVariableRef CVarWaveIntervalScale(TEXT("cc.Survival.WaveIntervalScale"), GWaveIntervalScale,
		TEXT("生存模式：波次间隔倍率（下一波起生效），默认 1。"));
	FAutoConsoleVariableRef CVarSpawnCountScale(TEXT("cc.Survival.SpawnCountScale"), GSpawnCountScale,
		TEXT("生存模式：每波数量倍率，默认 1。"));
	FAutoConsoleVariableRef CVarExperienceScale(TEXT("cc.Survival.XPScale"), GExperienceScale,
		TEXT("生存模式：击杀经验倍率，默认 1。"));
	FAutoConsoleVariableRef CVarSurvivalDebug(TEXT("cc.Survival.Debug"), GSurvivalDebug,
		TEXT("生存模式：1 = 屏幕显示波次、经验、复活倒计时等调试信息。"));

	/** 取当前世界的生存模式 GameMode；不在服务器或不是生存模式时提示并返回空。 */
	ACC_SurvivalGameMode* GetSurvivalMode(UWorld* World)
	{
		ACC_SurvivalGameMode* Mode = World ? World->GetAuthGameMode<ACC_SurvivalGameMode>() : nullptr;
		if (!Mode)
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] 调试命令需要在服务器（单机 / 主机）的生存模式地图中执行。"));
		}
		return Mode;
	}

	UCC_WaveDirectorComponent* GetWaves(UWorld* World)
	{
		ACC_SurvivalGameMode* Mode = GetSurvivalMode(World);
		ACC_SurvivalGameState* State = Mode ? Mode->GetSurvivalGameState() : nullptr;
		return State ? State->GetWaveDirector() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSetWave(TEXT("cc.Survival.SetWave"), TEXT("跳到第 N 波并立即开始：cc.Survival.SetWave 10"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UCC_WaveDirectorComponent* Waves = GetWaves(World); Waves && Args.Num() > 0)
			{
				Waves->DebugSetWave(FCString::Atoi(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSkipWave(TEXT("cc.Survival.SkipWave"), TEXT("立即开始下一波"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UCC_WaveDirectorComponent* Waves = GetWaves(World))
			{
				Waves->DebugSkipWave();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdClearEnemies(TEXT("cc.Survival.ClearEnemies"), TEXT("销毁所有波次怪物（不给经验）"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UCC_WaveDirectorComponent* Waves = GetWaves(World))
			{
				Waves->DebugClearEnemies();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAddXP(TEXT("cc.Survival.AddXP"), TEXT("给全队加经验：cc.Survival.AddXP 100"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ACC_SurvivalGameMode* Mode = GetSurvivalMode(World); Mode && Args.Num() > 0)
			{
				Mode->DebugAddExperience(FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGod(TEXT("cc.Survival.God"), TEXT("切换所有玩家的伤害免疫（Status.DamageImmunity）"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACC_SurvivalGameMode* Mode = GetSurvivalMode(World))
			{
				Mode->DebugToggleGodMode();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdReviveAll(TEXT("cc.Survival.ReviveAll"), TEXT("立即复活所有阵亡玩家"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACC_SurvivalGameMode* Mode = GetSurvivalMode(World))
			{
				Mode->DebugReviveAll();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdEndMatch(TEXT("cc.Survival.EndMatch"), TEXT("立即结算"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACC_SurvivalGameMode* Mode = GetSurvivalMode(World))
			{
				Mode->EndMatch();
			}
		}));
}
#endif

namespace CCSurvivalCVars
{
#if !UE_BUILD_SHIPPING
	float GetWaveIntervalScale() { return FMath::Max(0.05f, GWaveIntervalScale); }
	float GetSpawnCountScale() { return FMath::Max(0.f, GSpawnCountScale); }
	float GetExperienceScale() { return FMath::Max(0.f, GExperienceScale); }
	bool IsDebugDisplayEnabled() { return GSurvivalDebug != 0; }
#else
	float GetWaveIntervalScale() { return 1.f; }
	float GetSpawnCountScale() { return 1.f; }
	float GetExperienceScale() { return 1.f; }
	bool IsDebugDisplayEnabled() { return false; }
#endif
}
