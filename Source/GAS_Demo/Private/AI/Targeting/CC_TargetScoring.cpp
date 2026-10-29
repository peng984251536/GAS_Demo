#include "AI/Targeting/CC_TargetScoring.h"

float FCC_TargetScoring::Score(const FCC_TargetCandidate& Candidate, const FCC_TargetScoringParams& Params,
	bool bSelfIsMelee)
{
	float Result = Candidate.Distance;
	if (bSelfIsMelee)
	{
		const int32 Excess = FMath::Max(0, Candidate.MeleeAttackers - Params.MeleeSoftCap);
		Result += Params.CrowdPenalty * Excess;
	}
	if (Candidate.bTargetsSelf)
	{
		Result -= Params.RetaliationBonus;
	}
	if (Candidate.bBlocked)
	{
		Result += Params.BlockedPenalty;
	}
	return Result;
}

int32 FCC_TargetScoring::SelectBest(TConstArrayView<FCC_TargetCandidate> Candidates, int32 CurrentIndex,
	const FCC_TargetScoringParams& Params, float SwitchHysteresis, bool bSelfIsMelee)
{
	int32 BestIndex = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const float CandidateScore = Score(Candidates[Index], Params, bSelfIsMelee);
		if (CandidateScore < BestScore)
		{
			BestScore = CandidateScore;
			BestIndex = Index;
		}
	}

	// 当前目标仍在候选中时，分数优势不足迟滞值就不切换，防止目标来回跳。
	if (Candidates.IsValidIndex(CurrentIndex) && BestIndex != CurrentIndex)
	{
		const float CurrentScore = Score(Candidates[CurrentIndex], Params, bSelfIsMelee);
		if (CurrentScore - BestScore < SwitchHysteresis)
		{
			return CurrentIndex;
		}
	}
	return BestIndex;
}
