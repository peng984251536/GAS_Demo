// Fill out your copyright notice in the Description page of Project Settings.


#include "Data\AbilityData\CombatActionSet.h"

/**
 * 找到连击的某个动作
 * @param ComboIndex 
 * @return 
 */
UCombatActionData* UCombatActionSet::FindActionByComboIndex(int32 ComboIndex) const
{
	for (int idx = 0; idx < Actions.Num(); idx++)
	{
		if(idx == ComboIndex)
		{
			return Actions[idx].Get();
		}
	}
	
	return nullptr;
}
int UCombatActionSet::FindActionIndexByCombo(UCombatActionData* Data) const
{
	if (!IsValid(Data))
	{
		return -1;
	}

	for (int idx = 0; idx < Actions.Num(); idx++)
	{
		if(Actions[idx] == Data)
		{
			return idx;
		}
	}

	return -1;
}

/**
 * 获取这个连击有多少段
 * @return 
 */
int32 UCombatActionSet::GetComboLength() const
{
	return Actions.Num();
}



