// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraPopulationTypes.h"
#include "AuraCivilianWorkProfileRegistry.generated.h"

/** Immutable runtime view of the validated Civilian work-profile table. */
UCLASS(Transient)
class AURA_API UAuraCivilianWorkProfileRegistry : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(const TArray<FAuraCivilianWorkProfile>& InProfiles, FString& OutError);
	const FAuraCivilianWorkProfile* Find(FName WorkProfileId) const;
	bool IsInitialized() const { return bInitialized; }

private:
	UPROPERTY(Transient)
	TArray<FAuraCivilianWorkProfile> Profiles;

	bool bInitialized = false;
};
