#include "Animation/Notifies/AnimNotify_Footstep.h"

#include "Audio/CC_AudioLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"

// 检测脚下地面 → 按表面类型选事件 → 从全局配置取音效并在落脚点播放。
void UAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UWorld* World = IsValid(MeshComp) ? MeshComp->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector FootLocation = !FootBone.IsNone() && MeshComp->DoesSocketExist(FootBone)
		? MeshComp->GetSocketLocation(FootBone)
		: MeshComp->GetComponentLocation();

	// 起点略高于脚部，避免脚骨贴地或稍微陷进地面时检测不到。
	const FVector Start = FootLocation + FVector(0.f, 0.f, 20.f);
	const FVector End = FootLocation - FVector(0.f, 0.f, TraceDistance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CC_Footstep), false, MeshComp->GetOwner());
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	FName Event = DefaultEvent;
	FVector SoundLocation = FootLocation;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		SoundLocation = Hit.ImpactPoint;
		const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
		if (const FName* Found = SurfaceEvents.Find(Surface); Found && !Found->IsNone())
		{
			Event = *Found;
		}
	}

	if (USoundBase* Sound = UCC_AudioLibrary::PickGlobalAudioSound(Event))
	{
		const float Pitch = 1.f + FMath::FRandRange(-PitchVariation, PitchVariation);
		UGameplayStatics::PlaySoundAtLocation(MeshComp, Sound, SoundLocation, VolumeMultiplier, Pitch);
	}
}

FString UAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return FootBone.IsNone() ? TEXT("Footstep") : FString::Printf(TEXT("Footstep (%s)"), *FootBone.ToString());
}
