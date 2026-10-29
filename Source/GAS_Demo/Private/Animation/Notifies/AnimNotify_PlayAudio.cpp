#include "Animation/Notifies/AnimNotify_PlayAudio.h"

#include "Audio/CC_AudioLibrary.h"
#include "Components/SkeletalMeshComponent.h"

// 跟随模式附着到网格体（可指定插槽）；否则在插槽 / 网格体当前位置播一次。专用服务器在音效库内部跳过。
void UAnimNotify_PlayAudio::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!IsValid(MeshComp) || Event.IsNone())
	{
		return;
	}

	const bool bHasSocket = !SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName);
	if (bFollowMesh)
	{
		UCC_AudioLibrary::PlayAudioAttached(Event, MeshComp, bHasSocket ? SocketName : NAME_None, VolumeMultiplier);
	}
	else
	{
		const FVector Location = bHasSocket ? MeshComp->GetSocketLocation(SocketName) : MeshComp->GetComponentLocation();
		UCC_AudioLibrary::PlayAudioAtLocation(MeshComp, Event, Location, VolumeMultiplier);
	}
}

FString UAnimNotify_PlayAudio::GetNotifyName_Implementation() const
{
	return Event.IsNone() ? TEXT("Play Audio") : Event.ToString();
}
