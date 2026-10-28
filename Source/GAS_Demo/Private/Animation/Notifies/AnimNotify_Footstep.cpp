#include "Animation/Notifies/AnimNotify_Footstep.h"

#include "Audio/CC_AudioLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// 检测脚下地面 → 到全局配置按表面类型查事件 → 取音效并在落脚点播放。
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
	// 复杂碰撞检测才能读到材质（Material）里设置的 Phys Material；每一步只打一条射线，开销可忽略。
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CC_Footstep), true, MeshComp->GetOwner());
	Params.bReturnPhysicalMaterial = true;

	const UCC_AudioConfig* Config = UCC_AudioLibrary::GetAudioConfig();
	if (!Config)
	{
		return;
	}

	FHitResult Hit;
	FVector SoundLocation = FootLocation;
	EPhysicalSurface Surface = SurfaceType_Default;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		SoundLocation = Hit.ImpactPoint;
		Surface = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	}
	const FName Event = EventOverride.IsNone() ? Config->GetFootstepEvent(Surface) : EventOverride;

	UCC_AudioLibrary::PlayAudioAtLocation(MeshComp, Event, SoundLocation, VolumeMultiplier);
}

FString UAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return FootBone.IsNone() ? TEXT("Footstep") : FString::Printf(TEXT("Footstep (%s)"), *FootBone.ToString());
}
