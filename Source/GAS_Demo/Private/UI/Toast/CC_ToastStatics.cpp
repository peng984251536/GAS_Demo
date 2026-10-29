#include "UI/Toast/CC_ToastStatics.h"
#include "UI/Toast/CC_ToastSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** 从上下文对象找到明确所属的本地玩家；找不到返回空（表示发给全部本地玩家）。 */
	ULocalPlayer* FindOwningLocalPlayer(const UObject* Context)
	{
		const APlayerController* PC = nullptr;
		if (const UUserWidget* Widget = Cast<UUserWidget>(Context))
		{
			PC = Widget->GetOwningPlayer();
		}
		else
		{
			const AActor* Actor = Cast<AActor>(Context);
			if (!Actor)
			{
				if (const UActorComponent* Component = Cast<UActorComponent>(Context)) Actor = Component->GetOwner();
			}
			PC = Cast<APlayerController>(Actor);
			if (!PC)
			{
				if (const APawn* Pawn = Cast<APawn>(Actor)) PC = Cast<APlayerController>(Pawn->GetController());
			}
		}
		return PC && PC->IsLocalController() ? PC->GetLocalPlayer() : nullptr;
	}

	/** 解析目标玩家后对每个人执行 Func。返回执行次数。 */
	template <typename FuncType>
	int32 ForEachTarget(const UObject* Context, FuncType&& Func)
	{
		if (ULocalPlayer* Owner = FindOwningLocalPlayer(Context))
		{
			if (UCC_ToastSubsystem* Toasts = Owner->GetSubsystem<UCC_ToastSubsystem>())
			{
				Func(*Toasts);
				return 1;
			}
			return 0;
		}
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (!GameInstance) return 0;
		int32 Count = 0;
		for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (UCC_ToastSubsystem* Toasts = LocalPlayer ? LocalPlayer->GetSubsystem<UCC_ToastSubsystem>() : nullptr)
			{
				Func(*Toasts);
				++Count;
			}
		}
		return Count;
	}
}

bool UCC_ToastStatics::ShowToast(const UObject* WorldContextObject, const FCC_ToastRequest& Request)
{
	if (Request.Title.IsEmpty() && Request.Message.IsEmpty()) return false;
	return ForEachTarget(WorldContextObject, [&Request](UCC_ToastSubsystem& Toasts) { Toasts.ShowToast(Request); }) > 0;
}

bool UCC_ToastStatics::ShowToastText(const UObject* WorldContextObject, FText Title, FText Message,
	ECC_ToastStyle Style, float Duration, FName DedupeKey)
{
	FCC_ToastRequest Request;
	Request.Title = MoveTemp(Title);
	Request.Message = MoveTemp(Message);
	Request.Style = Style;
	Request.Duration = Duration;
	Request.DedupeKey = DedupeKey;
	return ShowToast(WorldContextObject, Request);
}

void UCC_ToastStatics::ClearToasts(const UObject* WorldContextObject)
{
	ForEachTarget(WorldContextObject, [](UCC_ToastSubsystem& Toasts) { Toasts.ClearToasts(); });
}
