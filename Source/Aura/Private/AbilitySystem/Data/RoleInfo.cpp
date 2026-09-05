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
		const FGameplayTag EntityPlayer = FGameplayTag::RequestGameplayTag(TEXT("Entity.Player"), false);
		const FGameplayTag ControlPlayer = FGameplayTag::RequestGameplayTag(TEXT("Control.Player"), false);
		const FGameplayTag EntityAmbient = FGameplayTag::RequestGameplayTag(TEXT("Entity.AmbientNPC"), false);
		const FGameplayTag ControlCivilianAI = FGameplayTag::RequestGameplayTag(TEXT("Control.CivilianAI"), false);
		const FGameplayTag FactionCivilian = FGameplayTag::RequestGameplayTag(TEXT("Faction.Civilian"), false);
		const FGameplayTag CombatCivilian = FGameplayTag::RequestGameplayTag(TEXT("Combat.Civilian"), false);
		const bool bPlayerIdentity = Info->EntityType.MatchesTagExact(EntityPlayer)
			&& Info->ControlType.MatchesTagExact(ControlPlayer);
		const bool bPlayableCivilian = Role == TEXT("Civilian")
			&& Info->EntityType.MatchesTagExact(EntityAmbient)
			&& Info->ControlType.MatchesTagExact(ControlCivilianAI)
			&& Info->Faction.MatchesTagExact(FactionCivilian)
			&& Info->CombatProfile.MatchesTagExact(CombatCivilian)
			&& !Info->bCanAttack;
		return IsRoleConfigured(Role) && Info->bPlayerSelectable && (bPlayerIdentity || bPlayableCivilian);
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
