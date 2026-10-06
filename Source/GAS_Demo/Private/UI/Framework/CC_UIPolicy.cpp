#include "UI/Framework/CC_UIPolicy.h"
#include "UI/Framework/CC_RootLayout.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
UCC_UIPolicy::UCC_UIPolicy() { DefaultRootLayoutClass = UCC_RootLayout::StaticClass(); }
UCC_RootLayout* UCC_UIPolicy::GetRootLayout(ULocalPlayer* Player) const { return Roots.FindRef(Player); }
UCC_RootLayout* UCC_UIPolicy::EnsureRoot(ULocalPlayer* Player)
{
 if (bUpdating || !Player) return nullptr;
 APlayerController* Controller = Player->PlayerController;
 if (!IsValid(Controller) || !Controller->IsLocalController() || !Controller->GetWorld() ||
  Controller->GetWorld()->bIsTearingDown || !FLocalPlayerContext(Player).IsValid()) return nullptr;
 TGuardValue<bool> Guard(bUpdating, true);
 UCC_RootLayout* Root = GetRootLayout(Player);
 if (!Root)
 {
  if (!DefaultRootLayoutClass || DefaultRootLayoutClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
  Root = CreateWidget<UCC_RootLayout>(Controller, DefaultRootLayoutClass);
  if (!Root) return nullptr;
  Roots.Add(Player, Root);
 }
 else if (AttachedControllers.FindRef(Player).Get() != Controller || AttachedWorlds.FindRef(Player).Get() != Controller->GetWorld())
 {
  Root->DetachFromViewport();
 }
 // 不锁定旧世界；LocalPlayer 动态提供新世界/Controller。
 Root->SetPlayerContext(FLocalPlayerContext(Player));
 if (!Root->IsInViewport() && !Root->AddToPlayerScreen(1000)) return nullptr;
 Root->ResumeLayout();
 AttachedWorlds.Add(Player, Controller->GetWorld());
 AttachedControllers.Add(Player, Controller);
 Root->ActivateWidget();
 return Root;
}
void UCC_UIPolicy::DetachWorld(UWorld* World)
{
 if (bUpdating) return;
 TGuardValue<bool> Guard(bUpdating, true);
 for (const auto& Pair : Roots)
 {
  if (AttachedWorlds.FindRef(Pair.Key).Get() != World) continue;
  // 释放页面对象池和延迟一帧的 Slate 引用，避免跨图保留旧世界。
  Pair.Value->DetachFromViewport();
  AttachedWorlds.Remove(Pair.Key);
  AttachedControllers.Remove(Pair.Key);
 }
}
void UCC_UIPolicy::ReleaseRoot(UCC_RootLayout* Root)
{
 if (!Root) return;
 Root->Shutdown();
 Root->RemoveFromParent();
}
void UCC_UIPolicy::ReleasePlayer(ULocalPlayer* Player)
{
 TObjectPtr<UCC_RootLayout> Root;
 AttachedWorlds.Remove(Player);
 AttachedControllers.Remove(Player);
 if (Roots.RemoveAndCopyValue(Player, Root)) ReleaseRoot(Root);
}
void UCC_UIPolicy::ReleaseAll()
{
 TGuardValue<bool> Guard(bUpdating, true);
 auto Previous = MoveTemp(Roots);
 Roots.Empty();
 AttachedWorlds.Empty();
 AttachedControllers.Empty();
 for (const auto& Pair : Previous) ReleaseRoot(Pair.Value);
}
