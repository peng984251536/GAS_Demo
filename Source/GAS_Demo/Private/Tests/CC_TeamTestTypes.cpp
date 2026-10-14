#include "Tests/CC_TeamTestTypes.h"
#include "GameplayTags/CC_Tags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UCC_TeamTestCharmEffect::UCC_TeamTestCharmEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantCharmed"));
	GEComponents.Add(Tags);
	FInheritedTagContainer Changes;
	Changes.AddTag(CCTags::Status::Charmed);
	Tags->SetAndApplyTargetTagChanges(Changes);
}
