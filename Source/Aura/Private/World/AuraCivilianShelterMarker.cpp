// Copyright Druid Mechanics

#include "World/AuraCivilianShelterMarker.h"

#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"

FGameplayTag AAuraCivilianShelterMarker::GetAuraTargetKind() const
{
	return FAuraGameplayTags::Get().Target_Kind_Shelter;
}

FText AAuraCivilianShelterMarker::GetAuraTargetDisplayName() const
{
	return FText::FromString(TEXT("Shelter"));
}

void AAuraCivilianShelterMarker::GetAuraInteractionOptions(const AActor* RequestingActor, TArray<FAuraInteractionOption>& OutOptions) const
{
	OutOptions.Reset();
	FAuraInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.OptionTag = FAuraGameplayTags::Get().Interaction_UseShelter;
	Option.DisplayText = FText::FromString(TEXT("Use Shelter"));
	Option.bEnabled = IsEnabled() && GetCapacity() > 0;
	Option.DisabledReason = Option.bEnabled ? EAuraInteractionResultCode::Success : EAuraInteractionResultCode::OptionUnavailable;
}

bool AAuraCivilianShelterMarker::ExecuteAuraInteraction(const AActor* RequestingActor, FGameplayTag OptionTag) const
{
	if (!IsValid(RequestingActor) || !OptionTag.MatchesTagExact(FAuraGameplayTags::Get().Interaction_UseShelter)) return false;
	if (!IsEnabled() || GetCapacity() <= 0) return false;
	UE_LOG(LogAura, Display, TEXT("[Interaction][Server] Executed UseShelter requester=%s target=%s marker=%s."),
		*GetNameSafe(RequestingActor), *GetNameSafe(this), *GetMarkerId().ToString());
	return true;
}
