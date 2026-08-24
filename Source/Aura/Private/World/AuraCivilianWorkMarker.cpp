// Copyright Druid Mechanics

#include "World/AuraCivilianWorkMarker.h"

bool AAuraCivilianWorkMarker::AcceptsWorkProfile(FName WorkProfileId) const
{
	return AllowedWorkProfileIds.Num() == 0 || AllowedWorkProfileIds.Contains(WorkProfileId);
}
