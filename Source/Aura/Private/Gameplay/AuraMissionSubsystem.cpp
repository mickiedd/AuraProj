// Copyright Druid Mechanics

#include "Gameplay/AuraMissionSubsystem.h"

#include "Engine/World.h"
#include "Gameplay/AuraEncounterCoordinator.h"
#include "Gameplay/AuraGameplayDefinitionRegistry.h"
#include "Gameplay/AuraMissionState.h"

void UAuraMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DefinitionRegistry = GetWorld() ? GetWorld()->GetSubsystem<UAuraGameplayDefinitionRegistry>() : nullptr;
	EncounterCoordinator = NewObject<UAuraEncounterCoordinator>(this, TEXT("MissionEncounterCoordinator"));
	RunState = FAuraMissionRunState();
	EntryBlocker = TEXT("DAY40_PACKAGED_EVIDENCE_MISSING");
}

void UAuraMissionSubsystem::Deinitialize()
{
	if (MissionStateActor)
	{
		MissionStateActor->Destroy();
		MissionStateActor = nullptr;
	}
	if (EncounterCoordinator) EncounterCoordinator->Reset();
	RunState = FAuraMissionRunState();
	Super::Deinitialize();
}

bool UAuraMissionSubsystem::IsAuthority() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UAuraMissionSubsystem::EnableGameplayProfile(FString& OutError)
{
	OutError.Reset();
	// Entry is transactional: every rejected prerequisite leaves the new profile
	// disabled and the world on the legacy path.
	bGameplayProfileEnabled = false;
	bRuntimeEntryReady = false;
	if (!IsAuthority())
	{
		OutError = TEXT("NotAuthority");
		return false;
	}
	if (!DefinitionRegistry)
	{
		OutError = TEXT("DefinitionRegistryMissing");
		return false;
	}
	if (!DefinitionRegistry->AreDefinitionsLoaded())
	{
		if (!DefinitionRegistry->LoadDefinitions(OutError)) return false;
	}
	// Definition parsing is not the inherited baseline gate. No caller can
	// accidentally activate mission gameplay from a Draft/unverified scope.
	if (!DefinitionRegistry->IsRuntimeReady())
	{
		bRuntimeEntryReady = false;
		EntryBlocker = DefinitionRegistry->GetRuntimeBlocker().IsEmpty()
			? TEXT("ANCHOR_SURVEY_UNVERIFIED") : DefinitionRegistry->GetRuntimeBlocker();
		OutError = EntryBlocker;
		return false;
	}
	bRuntimeEntryReady = false;
	EntryBlocker = TEXT("DAY40_PACKAGED_EVIDENCE_MISSING");
	OutError = EntryBlocker;
	return false;
}
