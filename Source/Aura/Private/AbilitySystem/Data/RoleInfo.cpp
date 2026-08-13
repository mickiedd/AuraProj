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

bool URoleInfo::IsPlayerRoleSelectable(FName Role) const
{
	if (const FRoleDefaultInfo* Info = RoleInformation.Find(Role))
	{
		return IsRoleConfigured(Role)
			&& Info->bPlayerSelectable
			&& Info->EntityType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Entity.Player"), false))
			&& Info->ControlType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Control.Player"), false));
	}
	return false;
}

FString FAuraRoleLoadResult::ToLogString() const
{
	FString Report = FString::Printf(TEXT("detectedVersion=%d publishedVersion=%d canPublish=%s issues=%d"),
		DetectedVersion, PublishedVersion, bCanPublish ? TEXT("true") : TEXT("false"), Issues.Num());
	for (const FAuraRoleValidationIssue& Issue : Issues)
	{
		Report += FString::Printf(TEXT("\n  [%s] role=%s path=%s: %s"),
			Issue.Severity == EAuraRoleValidationSeverity::Error ? TEXT("Error") : TEXT("Warning"),
			Issue.RoleId.IsNone() ? TEXT("<top-level>") : *Issue.RoleId.ToString(),
			*Issue.JsonPath,
			*Issue.Message);
	}
	return Report;
}
