#include "UI/Framework/CC_UIInputData.h"
#include "CommonUITypes.h"
#include "Engine/DataTable.h"

namespace
{
	// UE 5.6 的输入字段为 protected；通过派生行构造，再将基类部分复制到官方数据表。
	/** 将显示名称、键盘键、手柄键写入官方动作行，仅用于构造默认值。 */
	struct FCC_DefaultUIActionRow : FCommonInputActionDataBase
	{
		/** 以三个参数配置一条可由 CommonUI 路由的动作。 */
		FCC_DefaultUIActionRow(const FText& Name, FKey KeyboardKey, FKey GamepadKey)
		{
			DisplayName = Name;
			KeyboardInputTypeInfo.SetKey(KeyboardKey);
			DefaultGamepadInputTypeInfo.SetKey(GamepadKey);
		}
	};
}

// 构造原生默认动作表，复用官方 CommonUI InputData 的确认/返回机制。
UCC_UIInputData::UCC_UIInputData()
{
	// 默认子对象由 InputData 持有，其生命周期与动作配置一致。
	UDataTable* Table = CreateDefaultSubobject<UDataTable>(TEXT("DefaultUIActions"));
	Table->RowStruct = FCommonInputActionDataBase::StaticStruct();
	const FCommonInputActionDataBase Accept = FCC_DefaultUIActionRow(NSLOCTEXT("CCUI", "Accept", "Confirm"), EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom);
	Table->AddRow(TEXT("Accept"), Accept);
	const FCommonInputActionDataBase Back = FCC_DefaultUIActionRow(NSLOCTEXT("CCUI", "Back", "Back"), EKeys::Escape, EKeys::Gamepad_FaceButton_Right);
	Table->AddRow(TEXT("Back"), Back);
	DefaultClickAction.DataTable = Table;
	DefaultClickAction.RowName = TEXT("Accept");
	DefaultBackAction.DataTable = Table;
	DefaultBackAction.RowName = TEXT("Back");
}
