// Copyright Druid Mechanics


#include "AbilitySystem/Data/RoleInfo.h"

TArray<FName> URoleInfo::GetRoleNames() const
{
	TArray<FName> Names;
	RoleInformation.GenerateKeyArray(Names);
	return Names;
}

FRoleDefaultInfo URoleInfo::GetRoleDefaultInfo(FName Role) const
{
	return RoleInformation.FindChecked(Role);
}

bool URoleInfo::IsRoleConfigured(FName Role) const
{
	if (const FRoleDefaultInfo* Info = RoleInformation.Find(Role))
	{
		return Info->SkeletalMesh != nullptr && Info->AnimBlueprintClass != nullptr;
	}
	return false;
}