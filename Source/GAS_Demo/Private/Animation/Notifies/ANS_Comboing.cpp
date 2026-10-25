#include "Animation/Notifies/ANS_Comboing.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTags/CC_Tags.h"

namespace
{
UAbilitySystemComponent* GetComboArmorASC(USkeletalMeshComponent* MeshComp)
{
    if (!IsValid(MeshComp)) return nullptr;
    const UWorld* World = MeshComp->GetWorld();
    ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(MeshComp->GetOwner());
    if (!World || !World->IsGameWorld() || !IsValid(Character) || !Character->HasAuthority())
        return nullptr;
    return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character);
}
}

void UANS_Comboing::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    if (UAbilitySystemComponent* ASC = GetComboArmorASC(MeshComp))
    {
        // Loose tags use a separate contribution from GE-granted armor. Never reset the total count.
        ASC->AddLooseGameplayTag(CCTags::Status::SuperArmor);
    }
}

void UANS_Comboing::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);
    if (UAbilitySystemComponent* ASC = GetComboArmorASC(MeshComp))
    {
        // Montage interruption also ends the active notify state.
        ASC->RemoveLooseGameplayTag(CCTags::Status::SuperArmor);
    }
}
