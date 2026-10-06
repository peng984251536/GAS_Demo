#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "UI/Framework/CC_UIPolicy.h"
#include "UI/Framework/CC_RootLayout.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"
bool UCC_UIManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
 return !CastChecked<UGameInstance>(Outer)->IsDedicatedServerInstance();
}
void UCC_UIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 UClass* Class = DefaultUIPolicyClass.LoadSynchronous();
 if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) Class = UCC_UIPolicy::StaticClass();
 Policy = NewObject<UCC_UIPolicy>(this, Class);
 PlayerAddedHandle = GetGameInstance()->OnLocalPlayerAddedEvent.AddUObject(this, &ThisClass::HandlePlayerAdded);
 PlayerRemovedHandle = GetGameInstance()->OnLocalPlayerRemovedEvent.AddUObject(this, &ThisClass::HandlePlayerRemoved);
 WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldTearDown);
 MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandleMapLoaded);
 for (ULocalPlayer* Player : GetGameInstance()->GetLocalPlayers()) HandlePlayerAdded(Player);
}
UCC_RootLayout* UCC_UIManagerSubsystem::GetRootLayout(ULocalPlayer* Player) const
{
 return Policy ? Policy->GetRootLayout(Player) : nullptr;
}
UCC_RootLayout* UCC_UIManagerSubsystem::GetRootLayoutForPlayer(APlayerController* Player)
{
 if (!IsValid(Player) || !Player->IsLocalController() || !Player->GetGameInstance()) return nullptr;
 const auto* Manager = Player->GetGameInstance()->GetSubsystem<UCC_UIManagerSubsystem>();
 UCC_RootLayout* Root = Manager ? Manager->GetRootLayout(Player->GetLocalPlayer()) : nullptr;
 return Root && !Root->IsShuttingDown() && Root->IsLayoutAttached() && Root->GetOwningPlayer() == Player ? Root : nullptr;
}
void UCC_UIManagerSubsystem::HandlePlayerAdded(ULocalPlayer* Player)
{
 if (!Player || ControllerHandles.Contains(Player)) return;
 ControllerHandles.Add(Player, Player->OnPlayerControllerChanged().AddUObject(this, &ThisClass::HandleControllerChanged, Player));
 HandleControllerChanged(Player->PlayerController, Player);
}
void UCC_UIManagerSubsystem::HandleControllerChanged(APlayerController* Controller, ULocalPlayer* Player)
{
 if (!Policy || !IsValid(Controller) || Controller->GetGameInstance() != GetGameInstance()) return;
 if (UCC_RootLayout* Root = Policy->EnsureRoot(Player)) OnRootLayoutReady.Broadcast(Controller, Root);
}
void UCC_UIManagerSubsystem::HandlePlayerRemoved(ULocalPlayer* Player)
{
 FDelegateHandle Handle;
 if (Player && ControllerHandles.RemoveAndCopyValue(Player, Handle)) Player->OnPlayerControllerChanged().Remove(Handle);
 if (Policy) Policy->ReleasePlayer(Player);
}
void UCC_UIManagerSubsystem::HandleWorldTearDown(UWorld* World)
{
 if (Policy && World && World->GetGameInstance() == GetGameInstance()) Policy->DetachWorld(World);
}
void UCC_UIManagerSubsystem::HandleMapLoaded(UWorld* World)
{
 if (!World || World->GetGameInstance() != GetGameInstance()) return;
 for (ULocalPlayer* Player : GetGameInstance()->GetLocalPlayers()) HandleControllerChanged(Player->PlayerController, Player);
}
void UCC_UIManagerSubsystem::Deinitialize()
{
 GetGameInstance()->OnLocalPlayerAddedEvent.Remove(PlayerAddedHandle);
 GetGameInstance()->OnLocalPlayerRemovedEvent.Remove(PlayerRemovedHandle);
 FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
 FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
 for (const auto& Pair : ControllerHandles)
  if (ULocalPlayer* Player = Pair.Key.Get()) Player->OnPlayerControllerChanged().Remove(Pair.Value);
 ControllerHandles.Empty();
 UCC_UIPolicy* Previous = Policy;
 Policy = nullptr; // 页面退出回调不能重新取得策略并创建 UI。
 if (Previous) Previous->ReleaseAll();
 OnRootLayoutReady.Clear();
 Super::Deinitialize();
}

