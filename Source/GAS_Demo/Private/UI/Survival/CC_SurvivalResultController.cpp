#include "UI/Survival/CC_SurvivalResultController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_SurvivalSaveGame.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "Game/Survival/CC_TeamProgressionComponent.h"
#include "Game/Survival/CC_WaveDirectorComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UCC_SurvivalResultController::UCC_SurvivalResultController()
{
	ModelClass = UCC_SurvivalResultModel::StaticClass();
}

void UCC_SurvivalResultController::OnActivated()
{
	Super::OnActivated();
	UCC_SurvivalResultModel* ResultModel = Cast<UCC_SurvivalResultModel>(GetModel());
	if (!ResultModel)
	{
		return;
	}
	// 同一页面会话被其他页面覆盖后恢复时，保留已处理过的纪录结果，不重复读写存档。
	if (ResultModel->Result.bRecordLoaded)
	{
		FCC_SurvivalResultSnapshot Refreshed = BuildSnapshot();
		Refreshed.bRecordLoaded = true;
		Refreshed.BestWave = ResultModel->Result.BestWave;
		Refreshed.BestKills = ResultModel->Result.BestKills;
		Refreshed.BestSurvivalTime = ResultModel->Result.BestSurvivalTime;
		Refreshed.bNewBestWave = ResultModel->Result.bNewBestWave;
		Refreshed.bNewBestKills = ResultModel->Result.bNewBestKills;
		Refreshed.bNewBestTime = ResultModel->Result.bNewBestTime;
		ResultModel->SetResult(Refreshed);
		return;
	}
	ResultModel->SetResult(BuildSnapshot());

	const FGuid Token = GetActivationToken();
	const TWeakObjectPtr<UCC_SurvivalResultController> WeakThis(this);
	UGameplayStatics::AsyncLoadGameFromSlot(UCC_SurvivalSaveGame::SlotName(), GetLocalUserIndex(),
		FAsyncLoadGameFromSlotDelegate::CreateLambda([WeakThis, Token](const FString& SlotName, const int32 UserIndex, USaveGame* Loaded)
		{
			if (UCC_SurvivalResultController* Controller = WeakThis.Get())
			{
				Controller->HandleRecordLoaded(SlotName, UserIndex, Loaded, Token);
			}
		}));
}

FCC_SurvivalResultSnapshot UCC_SurvivalResultController::BuildSnapshot() const
{
	FCC_SurvivalResultSnapshot Snapshot;
	const UWorld* World = GetWorld();
	if (const ACC_SurvivalGameState* SurvivalState = World ? World->GetGameState<ACC_SurvivalGameState>() : nullptr)
	{
		Snapshot.Wave = SurvivalState->GetWaveDirector()->GetCurrentWave();
		Snapshot.TeamLevel = SurvivalState->GetProgression()->GetTeamLevel();
		Snapshot.TeamKills = SurvivalState->GetTeamStat(CCSurvivalTags::StatKills);
		Snapshot.SurvivalTime = SurvivalState->GetMatchElapsedSeconds();
	}
	const APlayerController* Player = GetPlayerController();
	if (const ACC_SurvivalPlayerState* PS = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr)
	{
		Snapshot.Kills = PS->GetStat(CCSurvivalTags::StatKills);
		Snapshot.Deaths = PS->GetStat(CCSurvivalTags::StatDeaths);
		Snapshot.Revives = PS->GetStat(CCSurvivalTags::StatRevives);
		Snapshot.DamageDealt = PS->GetStat(CCSurvivalTags::StatDamageDealt);
		Snapshot.BuffsChosen = PS->GetStat(CCSurvivalTags::StatBuffsChosen);
		Snapshot.bCanControlMatch = PS->CanControlMatch();
	}
	return Snapshot;
}

void UCC_SurvivalResultController::HandleRecordLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* Loaded, FGuid Token)
{
	UCC_SurvivalResultModel* ResultModel = Cast<UCC_SurvivalResultModel>(GetModel());
	if (!ResultModel || !IsActivationCurrent(Token))
	{
		return; // 页面已关闭或已重新打开，丢弃旧结果（存档本身不受影响）
	}
	UCC_SurvivalSaveGame* Record = Cast<UCC_SurvivalSaveGame>(Loaded);
	if (!Record)
	{
		Record = Cast<UCC_SurvivalSaveGame>(UGameplayStatics::CreateSaveGameObject(UCC_SurvivalSaveGame::StaticClass()));
	}
	if (!Record)
	{
		return;
	}

	FCC_SurvivalResultSnapshot Snapshot = ResultModel->Result;
	Snapshot.bRecordLoaded = true;
	Snapshot.bNewBestWave = Snapshot.Wave > Record->BestWave;
	Snapshot.bNewBestKills = Snapshot.Kills > Record->BestKills;
	Snapshot.bNewBestTime = Snapshot.SurvivalTime > Record->BestSurvivalTime;
	Record->BestWave = FMath::Max(Record->BestWave, Snapshot.Wave);
	Record->BestKills = FMath::Max(Record->BestKills, Snapshot.Kills);
	Record->BestSurvivalTime = FMath::Max(Record->BestSurvivalTime, Snapshot.SurvivalTime);
	Record->SaveVersion = UCC_SurvivalSaveGame::CurrentVersion;
	Snapshot.BestWave = Record->BestWave;
	Snapshot.BestKills = Record->BestKills;
	Snapshot.BestSurvivalTime = Record->BestSurvivalTime;
	ResultModel->SetResult(Snapshot);

	if (Snapshot.bNewBestWave || Snapshot.bNewBestKills || Snapshot.bNewBestTime)
	{
		UGameplayStatics::AsyncSaveGameToSlot(Record, SlotName, UserIndex);
	}
}

int32 UCC_SurvivalResultController::GetLocalUserIndex() const
{
	const APlayerController* Player = GetPlayerController();
	const ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
}

bool UCC_SurvivalResultController::RequestPlayAgain()
{
	const APlayerController* Player = GetPlayerController();
	ACC_SurvivalPlayerState* PS = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr;
	if (!CanHandleActions() || !PS || !PS->CanControlMatch())
	{
		return false;
	}
	PS->ServerRequestPlayAgain();
	return true;
}

bool UCC_SurvivalResultController::RequestReturnToLobby()
{
	const APlayerController* Player = GetPlayerController();
	ACC_SurvivalPlayerState* PS = Player ? Player->GetPlayerState<ACC_SurvivalPlayerState>() : nullptr;
	if (!CanHandleActions() || !PS || !PS->CanControlMatch())
	{
		return false;
	}
	PS->ServerRequestReturnToLobby();
	return true;
}
