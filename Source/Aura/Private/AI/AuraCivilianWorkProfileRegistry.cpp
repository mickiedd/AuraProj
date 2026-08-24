// Copyright Druid Mechanics

#include "AI/AuraCivilianWorkProfileRegistry.h"

bool UAuraCivilianWorkProfileRegistry::Initialize(const TArray<FAuraCivilianWorkProfile>& InProfiles, FString& OutError)
{
	OutError.Empty();
	Profiles.Empty();
	TSet<FName> Seen;
	for (const FAuraCivilianWorkProfile& Profile : InProfiles)
	{
		if (Profile.WorkProfileId.IsNone() || Seen.Contains(Profile.WorkProfileId))
		{
			OutError = TEXT("Work-profile registry contains an empty or duplicate profile ID.");
			Profiles.Empty();
			bInitialized = false;
			return false;
		}
		if (Profile.MovementSpeed <= 0.f || Profile.WanderRadius <= 0.f || Profile.ObserveRadius <= 0.f
			|| Profile.FleeDistance <= 0.f || Profile.MoveTimeout <= 0.f || Profile.CalmDuration < 0.f)
		{
			OutError = FString::Printf(TEXT("Work profile '%s' contains an unsafe movement/threat range."), *Profile.WorkProfileId.ToString());
			Profiles.Empty();
			bInitialized = false;
			return false;
		}
		Seen.Add(Profile.WorkProfileId);
		Profiles.Add(Profile);
	}
	bInitialized = Profiles.Num() > 0;
	if (!bInitialized)
	{
		OutError = TEXT("No Civilian work profiles were published.");
	}
	return bInitialized;
}

const FAuraCivilianWorkProfile* UAuraCivilianWorkProfileRegistry::Find(FName WorkProfileId) const
{
	return Profiles.FindByPredicate([WorkProfileId](const FAuraCivilianWorkProfile& Profile)
	{
		return Profile.WorkProfileId == WorkProfileId;
	});
}
