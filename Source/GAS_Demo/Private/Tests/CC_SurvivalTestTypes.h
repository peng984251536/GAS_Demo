#pragma once
#include "Actors/Character/CC_EnemyCharacter.h"
#include "CC_SurvivalTestTypes.generated.h"

/**
 * 自动化测试用的敌人：测试世界不会执行 BeginPlay，这里公开生命耗尽事件的绑定入口。
 * 不出现在下拉列表中，不用于正式玩法。
 */
UCLASS(NotBlueprintable, HideDropdown)
class ACC_SurvivalTestEnemy : public ACC_EnemyCharacter
{
	GENERATED_BODY()
public:
	void TestBindOutOfHealth() { BindOutOfHealth(GetAttributeSet()); }
	void TestResetDeathReport() { ResetDeathReport(); }
};
