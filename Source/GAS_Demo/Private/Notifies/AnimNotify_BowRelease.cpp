#include "Notifies/AnimNotify_BowRelease.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GAS_Demo.h"
#include "NativeGameplayTags.h"

namespace
{
	// 原生注册此事件，后续射击能力可等待同名 Gameplay Event。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(BowReleaseEventTag, "CCTags.CCAbilityTrigger.BowRelease");
}

UAnimNotify_BowRelease::UAnimNotify_BowRelease()
{
	// 放箭影响实际战斗结果；蒙太奇播放到此帧时同步触发更准确。
	bIsNativeBranchingPoint = true;
}

void UAnimNotify_BowRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Notify] Skipped: Enemy=None Target=None Distance=-1.0 Montage=%s ReleaseTag=CCTags.CCAbilityTrigger.BowRelease Reason=MissingMesh"),
			*GetNameSafe(Animation));
		return;
	}
	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !IsValid(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner)))
	{
		UE_LOG(LogGAS_Demo, Warning,
			TEXT("[BowAI][Notify] Skipped: Enemy=%s Target=None Distance=-1.0 Montage=%s ReleaseTag=CCTags.CCAbilityTrigger.BowRelease Mesh=%s Reason=MissingOwnerOrASC"),
			*GetNameSafe(Owner), *GetNameSafe(Animation), *GetNameSafe(MeshComp));
		return;
	}
	const ACC_BaseCharacter* Enemy = Cast<ACC_BaseCharacter>(Owner);
	const AActor* Target = IsValid(Enemy) ? Enemy->GetClosestActor().Actor.Get() : nullptr;
	const float Distance = IsValid(Target) ? FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation()) : -1.0f;
	const FString Context = FString::Printf(
		TEXT("Enemy=%s Target=%s Distance=%.1f Montage=%s ReleaseTag=CCTags.CCAbilityTrigger.BowRelease"),
		*GetNameSafe(Owner), *GetNameSafe(Target), Distance, *GetNameSafe(Animation));
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Notify] Start: %s HeldArrow=%s Visible=%d Authority=%d"),
		*Context, *HeldArrowComponentName.ToString(), SetVisibility, Owner->HasAuthority());

	// Notify 可能在服务器和各客户端都执行。各端只隐藏自己的手持模型，
	// SetVisibility 不是投射物生成操作，也不依赖在共享的 Notify 对象里保存状态。
	if (!HeldArrowComponentName.IsNone())
	{
		bool bFoundHeldArrow = false;
		const FString ExpectedName = HeldArrowComponentName.ToString();
		TArray<UStaticMeshComponent*> MeshComponents;
		Owner->GetComponents<UStaticMeshComponent>(MeshComponents);
		for (UStaticMeshComponent* Component : MeshComponents)
		{
			if (IsValid(Component) &&
				(Component->GetFName() == HeldArrowComponentName ||
					Component->ComponentHasTag(HeldArrowComponentName) ||
					Component->GetName().StartsWith(ExpectedName + TEXT("_GEN_VARIABLE"))))
			{
				Component->SetVisibility(SetVisibility, true);
				bFoundHeldArrow = true;
				UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Notify] VisibilityChanged: %s Component=%s Visible=%d"),
					*Context, *GetNameSafe(Component), SetVisibility);
				break;
			}
		}
		if (!bFoundHeldArrow)
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Notify] HeldArrowMissing: %s Expected=%s"),
				*Context, *HeldArrowComponentName.ToString());
		}
	}

	// 客户端的动画通知只处理表现；放箭事件只交给服务器 ASC，避免多端重复生成箭。
	if (!Owner->HasAuthority() || SetVisibility)
	{
		UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Notify] ReleaseSkipped: %s Authority=%d Visible=%d"),
			*Context, Owner->HasAuthority(), SetVisibility);
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = BowReleaseEventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, BowReleaseEventTag, Payload);
	UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Notify] ReleaseEventSent: %s"), *Context);
}

FString UAnimNotify_BowRelease::GetNotifyName_Implementation() const
{
	return TEXT("Bow Release");
}
