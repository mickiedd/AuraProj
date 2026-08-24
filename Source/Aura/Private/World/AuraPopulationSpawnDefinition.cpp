// Copyright Druid Mechanics

#include "World/AuraPopulationSpawnDefinition.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AuraPopulationDefinitionPrivate
{
	bool GetRequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutValue, TArray<FString>& Errors, const FString& Path)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s.%s must be a non-empty string"), *Path, Field));
			return false;
		}
		OutValue.TrimStartAndEndInline();
		return true;
	}

	bool GetRequiredNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& OutValue, TArray<FString>& Errors, const FString& Path)
	{
		if (!Object.IsValid() || !Object->TryGetNumberField(Field, OutValue) || !FMath::IsFinite(OutValue))
		{
			Errors.Add(FString::Printf(TEXT("%s.%s must be a finite number"), *Path, Field));
			return false;
		}
		return true;
	}

	FName ParseName(const FString& Value)
	{
		return FName(*Value);
	}

	bool ParseNonNegativeNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValue, TArray<FString>& Errors, const FString& Path)
	{
		double Number = 0.0;
		if (!GetRequiredNumber(Object, Field, Number, Errors, Path)) return false;
		if (Number < 0.0)
		{
			Errors.Add(FString::Printf(TEXT("%s.%s must be non-negative"), *Path, Field));
			return false;
		}
		OutValue = static_cast<float>(Number);
		if (!FMath::IsFinite(OutValue))
		{
			Errors.Add(FString::Printf(TEXT("%s.%s is outside the supported float range"), *Path, Field));
			return false;
		}
		return true;
	}

	bool ParseOptionalNonNegativeNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValue, TArray<FString>& Errors, const FString& Path)
	{
		if (!Object.IsValid() || !Object->HasField(Field)) return true;
		return ParseNonNegativeNumber(Object, Field, OutValue, Errors, Path);
	}

	bool ParseOptionalTags(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FGameplayTagContainer& OutTags, TArray<FString>& Errors, const FString& Path)
	{
		if (!Object.IsValid() || !Object->HasField(Field)) return true;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || !Values)
		{
			Errors.Add(FString::Printf(TEXT("%s.%s must be an array when present"), *Path, Field));
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid() || Value->Type != EJson::String || Value->AsString().TrimStartAndEnd().IsEmpty())
			{
				Errors.Add(FString::Printf(TEXT("%s.%s must contain non-empty gameplay-tag strings"), *Path, Field));
				continue;
			}
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Value->AsString()), false);
			if (!Tag.IsValid())
			{
				Errors.Add(FString::Printf(TEXT("%s.%s contains an unregistered gameplay tag '%s'"), *Path, Field, *Value->AsString()));
				continue;
			}
			OutTags.AddTag(Tag);
		}
		return true;
	}

	bool ParseOptionalName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FName& OutValue, TArray<FString>& Errors, const FString& Path)
	{
		if (!Object.IsValid() || !Object->HasField(Field)) return true;
		FString Value;
		if (!Object->TryGetStringField(Field, Value) || Value.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s.%s must be a non-empty string when present"), *Path, Field));
			return false;
		}
		Value.TrimStartAndEndInline();
		OutValue = ParseName(Value);
		return !OutValue.IsNone();
	}
}

FString UAuraPopulationSpawnDefinition::GetPopulationTablePath()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("PopulationSpawnTable.json"));
}

FString UAuraPopulationSpawnDefinition::GetWorkProfileTablePath()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("CivilianWorkProfiles.json"));
}

const FAuraCivilianWorkProfile* UAuraPopulationSpawnDefinition::FindWorkProfile(FName WorkProfileId) const
{
	return WorkProfiles.FindByPredicate([WorkProfileId](const FAuraCivilianWorkProfile& Profile)
	{
		return Profile.WorkProfileId == WorkProfileId;
	});
}

bool UAuraPopulationSpawnDefinition::LoadJsonObject(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutError = FString::Printf(TEXT("Unable to read '%s'"), *FilePath);
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Unable to parse JSON '%s'"), *FilePath);
		return false;
	}
	return true;
}

bool UAuraPopulationSpawnDefinition::ParseWorkProfiles(const TSharedPtr<FJsonObject>& Root, TArray<FString>& OutErrors)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("workProfiles"), Values) || !Values)
	{
		OutErrors.Add(TEXT("CivilianWorkProfiles.json.workProfiles must be an array"));
		return false;
	}

	TSet<FName> SeenIds;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const FString Path = FString::Printf(TEXT("workProfiles[%d]"), Index);
		const TSharedPtr<FJsonObject> ProfileObject = (*Values)[Index].IsValid() ? (*Values)[Index]->AsObject() : nullptr;
		if (!ProfileObject.IsValid())
		{
			OutErrors.Add(Path + TEXT(" must be an object"));
			continue;
		}

		FString Id;
		if (!AuraPopulationDefinitionPrivate::GetRequiredString(ProfileObject, TEXT("id"), Id, OutErrors, Path)) continue;
		const FName ProfileId = AuraPopulationDefinitionPrivate::ParseName(Id);
		if (SeenIds.Contains(ProfileId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s.id duplicates '%s'"), *Path, *ProfileId.ToString()));
			continue;
		}
		SeenIds.Add(ProfileId);

		const TSharedPtr<FJsonObject>* MovementObject = nullptr;
		const TSharedPtr<FJsonObject>* ThreatObject = nullptr;
		const TSharedPtr<FJsonObject>* ScheduleObject = nullptr;
		if (!ProfileObject->TryGetObjectField(TEXT("movement"), MovementObject) || !MovementObject || !MovementObject->IsValid())
		{
			OutErrors.Add(Path + TEXT(".movement must be an object"));
			continue;
		}
		if (!ProfileObject->TryGetObjectField(TEXT("threat"), ThreatObject) || !ThreatObject || !ThreatObject->IsValid())
		{
			OutErrors.Add(Path + TEXT(".threat must be an object"));
			continue;
		}
		if (!ProfileObject->TryGetObjectField(TEXT("schedule"), ScheduleObject) || !ScheduleObject || !ScheduleObject->IsValid())
		{
			OutErrors.Add(Path + TEXT(".schedule must be an object"));
			continue;
		}

		FAuraCivilianWorkProfile Profile;
		Profile.WorkProfileId = ProfileId;
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*MovementObject, TEXT("speed"), Profile.MovementSpeed, OutErrors, Path + TEXT(".movement"));
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*MovementObject, TEXT("wanderRadius"), Profile.WanderRadius, OutErrors, Path + TEXT(".movement"));
		AuraPopulationDefinitionPrivate::ParseOptionalNonNegativeNumber(*MovementObject, TEXT("fleeSpeed"), Profile.FleeMovementSpeed, OutErrors, Path + TEXT(".movement"));
		AuraPopulationDefinitionPrivate::ParseOptionalNonNegativeNumber(*MovementObject, TEXT("moveTimeout"), Profile.MoveTimeout, OutErrors, Path + TEXT(".movement"));
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*ThreatObject, TEXT("observeRadius"), Profile.ObserveRadius, OutErrors, Path + TEXT(".threat"));
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*ThreatObject, TEXT("fleeDistance"), Profile.FleeDistance, OutErrors, Path + TEXT(".threat"));
		AuraPopulationDefinitionPrivate::ParseOptionalNonNegativeNumber(*ThreatObject, TEXT("calmDuration"), Profile.CalmDuration, OutErrors, Path + TEXT(".threat"));
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*ScheduleObject, TEXT("startHour"), Profile.ScheduleStartHour, OutErrors, Path + TEXT(".schedule"));
		AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*ScheduleObject, TEXT("endHour"), Profile.ScheduleEndHour, OutErrors, Path + TEXT(".schedule"));
		AuraPopulationDefinitionPrivate::ParseOptionalNonNegativeNumber(*ScheduleObject, TEXT("workDuration"), Profile.WorkDuration, OutErrors, Path + TEXT(".schedule"));
		AuraPopulationDefinitionPrivate::ParseOptionalNonNegativeNumber(*ScheduleObject, TEXT("wanderDuration"), Profile.WanderDuration, OutErrors, Path + TEXT(".schedule"));
		if (Profile.ScheduleStartHour > 24.f || Profile.ScheduleEndHour > 24.f || Profile.ScheduleStartHour > Profile.ScheduleEndHour)
		{
			OutErrors.Add(Path + TEXT(".schedule must use hours in [0,24] with startHour <= endHour"));
		}
		FString Phase;
		if (AuraPopulationDefinitionPrivate::GetRequiredString(*ScheduleObject, TEXT("phase"), Phase, OutErrors, Path + TEXT(".schedule")))
		{
			Profile.SchedulePhase = AuraPopulationDefinitionPrivate::ParseName(Phase);
		}
		const TSharedPtr<FJsonObject>* MarkerObject = nullptr;
		if (ProfileObject->TryGetObjectField(TEXT("markerTags"), MarkerObject) && MarkerObject && MarkerObject->IsValid())
		{
			AuraPopulationDefinitionPrivate::ParseOptionalTags(*MarkerObject, TEXT("work"), Profile.WorkMarkerTags, OutErrors, Path + TEXT(".markerTags"));
			AuraPopulationDefinitionPrivate::ParseOptionalTags(*MarkerObject, TEXT("observation"), Profile.ObservationMarkerTags, OutErrors, Path + TEXT(".markerTags"));
			AuraPopulationDefinitionPrivate::ParseOptionalTags(*MarkerObject, TEXT("shelter"), Profile.ShelterMarkerTags, OutErrors, Path + TEXT(".markerTags"));
		}
		WorkProfiles.Add(Profile);
	}
	return OutErrors.IsEmpty();
}

bool UAuraPopulationSpawnDefinition::ParsePopulationRows(const TSharedPtr<FJsonObject>& Root, TArray<FString>& OutErrors)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("populations"), Values) || !Values)
	{
		OutErrors.Add(TEXT("PopulationSpawnTable.json.populations must be an array"));
		return false;
	}

	TSet<FName> SeenIds;
	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const FString Path = FString::Printf(TEXT("populations[%d]"), Index);
		const TSharedPtr<FJsonObject> RowObject = (*Values)[Index].IsValid() ? (*Values)[Index]->AsObject() : nullptr;
		if (!RowObject.IsValid())
		{
			OutErrors.Add(Path + TEXT(" must be an object"));
			continue;
		}

		FString PopulationIdString;
		FString MapIdString;
		FString ZoneIdString;
		FString RoleIdString;
		FString ActorClassPath;
		FString DefaultWorkProfile;
		const bool bHasId = AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("populationId"), PopulationIdString, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("mapId"), MapIdString, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("zoneId"), ZoneIdString, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("roleId"), RoleIdString, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("actorClass"), ActorClassPath, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredString(RowObject, TEXT("defaultWorkProfileId"), DefaultWorkProfile, OutErrors, Path);
		if (!bHasId) continue;

		FAuraPopulationSpawnRow Row;
		Row.PopulationId = AuraPopulationDefinitionPrivate::ParseName(PopulationIdString);
		Row.MapId = AuraPopulationDefinitionPrivate::ParseName(MapIdString);
		Row.ZoneId = AuraPopulationDefinitionPrivate::ParseName(ZoneIdString);
		Row.RoleId = AuraPopulationDefinitionPrivate::ParseName(RoleIdString);
		Row.ActorClassPath = ActorClassPath;
		Row.DefaultWorkProfileId = AuraPopulationDefinitionPrivate::ParseName(DefaultWorkProfile);
		if (SeenIds.Contains(Row.PopulationId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s.populationId duplicates '%s'"), *Path, *Row.PopulationId.ToString()));
			continue;
		}
		SeenIds.Add(Row.PopulationId);

		const TArray<TSharedPtr<FJsonValue>>* VolumeValues = nullptr;
		if (!RowObject->TryGetArrayField(TEXT("spawnVolumeIds"), VolumeValues) || !VolumeValues || VolumeValues->Num() == 0)
		{
			OutErrors.Add(Path + TEXT(".spawnVolumeIds must contain at least one ID"));
		}
		else
		{
			TSet<FName> SeenVolumes;
			for (const TSharedPtr<FJsonValue>& VolumeValue : *VolumeValues)
			{
				if (!VolumeValue.IsValid() || VolumeValue->Type != EJson::String)
				{
					OutErrors.Add(Path + TEXT(".spawnVolumeIds must contain only strings"));
					continue;
				}
				const FName VolumeId = AuraPopulationDefinitionPrivate::ParseName(VolumeValue->AsString());
				if (VolumeId.IsNone() || SeenVolumes.Contains(VolumeId))
				{
					OutErrors.Add(Path + TEXT(".spawnVolumeIds contains an empty or duplicate ID"));
					continue;
				}
				SeenVolumes.Add(VolumeId);
				Row.SpawnVolumeIds.Add(VolumeId);
			}
		}

		double InitialCount = 0.0;
		double MaximumCount = 0.0;
		AuraPopulationDefinitionPrivate::GetRequiredNumber(RowObject, TEXT("initialCount"), InitialCount, OutErrors, Path);
		AuraPopulationDefinitionPrivate::GetRequiredNumber(RowObject, TEXT("maximumCount"), MaximumCount, OutErrors, Path);
		const bool bCountsFitInt32 = FMath::IsFinite(InitialCount) && FMath::IsFinite(MaximumCount)
			&& InitialCount <= 2147483647.0 && MaximumCount <= 2147483647.0;
		if (bCountsFitInt32)
		{
			Row.InitialCount = FMath::RoundToInt(static_cast<float>(InitialCount));
			Row.MaximumCount = FMath::RoundToInt(static_cast<float>(MaximumCount));
		}
		if (!bCountsFitInt32 || InitialCount < 0.0 || MaximumCount <= 0.0 || InitialCount > MaximumCount || !FMath::IsNearlyEqual(InitialCount, Row.InitialCount) || !FMath::IsNearlyEqual(MaximumCount, Row.MaximumCount))
		{
			OutErrors.Add(Path + TEXT(".initialCount/maximumCount must be whole numbers with 0 <= initialCount <= maximumCount and maximumCount > 0"));
		}

		bool bSpawnOnLoad = false;
		if (!RowObject->TryGetBoolField(TEXT("spawnOnLoad"), bSpawnOnLoad))
		{
			OutErrors.Add(Path + TEXT(".spawnOnLoad must be a boolean"));
		}
		Row.bSpawnOnLoad = bSpawnOnLoad;

		const TSharedPtr<FJsonObject>* RespawnObject = nullptr;
		if (!RowObject->TryGetObjectField(TEXT("respawnPolicy"), RespawnObject) || !RespawnObject || !RespawnObject->IsValid())
		{
			OutErrors.Add(Path + TEXT(".respawnPolicy must be an object"));
		}
		else
		{
			bool bEnabled = false;
			if (!(*RespawnObject)->TryGetBoolField(TEXT("enabled"), bEnabled)) OutErrors.Add(Path + TEXT(".respawnPolicy.enabled must be a boolean"));
			Row.RespawnPolicy.bEnabled = bEnabled;
			AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*RespawnObject, TEXT("delaySeconds"), Row.RespawnPolicy.DelaySeconds, OutErrors, Path + TEXT(".respawnPolicy"));
			AuraPopulationDefinitionPrivate::ParseNonNegativeNumber(*RespawnObject, TEXT("corpseSeconds"), Row.RespawnPolicy.CorpseSeconds, OutErrors, Path + TEXT(".respawnPolicy"));
			const TArray<TSharedPtr<FJsonValue>>* PhaseValues = nullptr;
			if (!(*RespawnObject)->TryGetArrayField(TEXT("allowedBattlePhases"), PhaseValues) || !PhaseValues)
			{
				OutErrors.Add(Path + TEXT(".respawnPolicy.allowedBattlePhases must be an array"));
			}
			else
			{
				for (const TSharedPtr<FJsonValue>& PhaseValue : *PhaseValues)
				{
					if (!PhaseValue.IsValid() || PhaseValue->Type != EJson::String)
					{
						OutErrors.Add(Path + TEXT(".respawnPolicy.allowedBattlePhases must contain only strings"));
						continue;
					}
					FString PhaseString = PhaseValue->AsString();
					PhaseString.TrimStartAndEndInline();
					const FName Phase = AuraPopulationDefinitionPrivate::ParseName(PhaseString);
					if (Phase.IsNone()) OutErrors.Add(Path + TEXT(".respawnPolicy.allowedBattlePhases contains an empty phase"));
					else Row.RespawnPolicy.AllowedBattlePhases.Add(Phase);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* OverrideValues = nullptr;
		if (RowObject->HasField(TEXT("memberOverrides"))
			&& (!RowObject->TryGetArrayField(TEXT("memberOverrides"), OverrideValues) || !OverrideValues))
		{
			OutErrors.Add(Path + TEXT(".memberOverrides must be an array"));
		}
		else if (OverrideValues)
		{
			TSet<int32> SeenSlots;
			for (int32 OverrideIndex = 0; OverrideIndex < OverrideValues->Num(); ++OverrideIndex)
			{
				const FString OverridePath = FString::Printf(TEXT("%s.memberOverrides[%d]"), *Path, OverrideIndex);
				const TSharedPtr<FJsonObject> OverrideObject = (*OverrideValues)[OverrideIndex].IsValid() ? (*OverrideValues)[OverrideIndex]->AsObject() : nullptr;
				if (!OverrideObject.IsValid())
				{
					OutErrors.Add(OverridePath + TEXT(" must be an object"));
					continue;
				}
				double SlotNumber = -1.0;
				AuraPopulationDefinitionPrivate::GetRequiredNumber(OverrideObject, TEXT("slotIndex"), SlotNumber, OutErrors, OverridePath);
				const bool bSlotFitsInt32 = FMath::IsFinite(SlotNumber) && SlotNumber >= 0.0 && SlotNumber <= 2147483647.0;
				const int32 SlotIndex = bSlotFitsInt32 ? FMath::RoundToInt(static_cast<float>(SlotNumber)) : INDEX_NONE;
				if (!bSlotFitsInt32 || SlotIndex < 0 || SlotIndex >= Row.MaximumCount || !FMath::IsNearlyEqual(SlotNumber, SlotIndex) || SeenSlots.Contains(SlotIndex))
				{
					OutErrors.Add(OverridePath + TEXT(".slotIndex must be unique and within the population capacity"));
				}
				SeenSlots.Add(SlotIndex);
				FAuraPopulationMemberOverride Override;
				Override.SlotIndex = SlotIndex;
				AuraPopulationDefinitionPrivate::ParseOptionalName(OverrideObject, TEXT("workProfileId"), Override.WorkProfileId, OutErrors, OverridePath);
				AuraPopulationDefinitionPrivate::ParseOptionalName(OverrideObject, TEXT("merchantDefinitionId"), Override.MerchantDefinitionId, OutErrors, OverridePath);
				if (Override.WorkProfileId.IsNone() && Override.MerchantDefinitionId.IsNone()) OutErrors.Add(OverridePath + TEXT(" must override workProfileId or merchantDefinitionId"));
				Row.MemberOverrides.Add(Override);
			}
		}

		PopulationRows.Add(Row);
	}
	return OutErrors.IsEmpty();
}

bool UAuraPopulationSpawnDefinition::ParseLevels(TArray<FString>& OutErrors)
{
	TSharedPtr<FJsonObject> Root;
	FString Error;
	const FString LevelPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("LevelConfig.json"));
	if (!LoadJsonObject(LevelPath, Root, Error))
	{
		OutErrors.Add(Error);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("levels"), Values) || !Values)
	{
		OutErrors.Add(TEXT("LevelConfig.json.levels must be an array"));
		return false;
	}
	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> LevelObject = (*Values)[Index].IsValid() ? (*Values)[Index]->AsObject() : nullptr;
		if (!LevelObject.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("LevelConfig.json.levels[%d] must be an object"), Index));
			continue;
		}
		FString Id;
		FString MapPath;
		if (!AuraPopulationDefinitionPrivate::GetRequiredString(LevelObject, TEXT("id"), Id, OutErrors, FString::Printf(TEXT("LevelConfig.json.levels[%d]"), Index))
			|| !AuraPopulationDefinitionPrivate::GetRequiredString(LevelObject, TEXT("mapPath"), MapPath, OutErrors, FString::Printf(TEXT("LevelConfig.json.levels[%d]"), Index)))
		{
			continue;
		}
		const FName LevelId = AuraPopulationDefinitionPrivate::ParseName(Id);
		if (LevelMapPaths.Contains(LevelId)) OutErrors.Add(FString::Printf(TEXT("LevelConfig.json duplicates level id '%s'"), *Id));
		else LevelMapPaths.Add(LevelId, MapPath);
	}
	return OutErrors.IsEmpty();
}

bool UAuraPopulationSpawnDefinition::LoadDefinitions(FString& OutError)
{
	OutError.Empty();
	SchemaVersion = 0;
	PopulationRows.Empty();
	WorkProfiles.Empty();
	LevelMapPaths.Empty();

	TArray<FString> Errors;
	TSharedPtr<FJsonObject> WorkRoot;
	TSharedPtr<FJsonObject> PopulationRoot;
	FString LoadError;
	if (!LoadJsonObject(GetWorkProfileTablePath(), WorkRoot, LoadError)) Errors.Add(LoadError);
	if (!LoadJsonObject(GetPopulationTablePath(), PopulationRoot, LoadError)) Errors.Add(LoadError);

	if (WorkRoot.IsValid())
	{
		double Version = 0.0;
		if (!AuraPopulationDefinitionPrivate::GetRequiredNumber(WorkRoot, TEXT("schemaVersion"), Version, Errors, TEXT("CivilianWorkProfiles.json")) || !FMath::IsNearlyEqual(Version, 1.0))
		{
			Errors.Add(TEXT("CivilianWorkProfiles.json.schemaVersion must be 1"));
		}
		ParseWorkProfiles(WorkRoot, Errors);
	}
	if (PopulationRoot.IsValid())
	{
		double Version = 0.0;
		if (!AuraPopulationDefinitionPrivate::GetRequiredNumber(PopulationRoot, TEXT("schemaVersion"), Version, Errors, TEXT("PopulationSpawnTable.json")) || !FMath::IsNearlyEqual(Version, 1.0))
		{
			Errors.Add(TEXT("PopulationSpawnTable.json.schemaVersion must be 1"));
		}
		SchemaVersion = FMath::IsNearlyEqual(Version, 1.0) ? 1 : 0;
		ParsePopulationRows(PopulationRoot, Errors);
	}
	ParseLevels(Errors);

	for (const FAuraPopulationSpawnRow& Row : PopulationRows)
	{
		if (!LevelMapPaths.Contains(Row.MapId)) Errors.Add(FString::Printf(TEXT("population '%s' references unknown mapId '%s'"), *Row.PopulationId.ToString(), *Row.MapId.ToString()));
		if (!FindWorkProfile(Row.DefaultWorkProfileId)) Errors.Add(FString::Printf(TEXT("population '%s' references unknown defaultWorkProfileId '%s'"), *Row.PopulationId.ToString(), *Row.DefaultWorkProfileId.ToString()));
		for (const FAuraPopulationMemberOverride& Override : Row.MemberOverrides)
		{
			if (!Override.WorkProfileId.IsNone() && !FindWorkProfile(Override.WorkProfileId)) Errors.Add(FString::Printf(TEXT("population '%s' override slot %d references unknown workProfileId '%s'"), *Row.PopulationId.ToString(), Override.SlotIndex, *Override.WorkProfileId.ToString()));
		}
	}

	if (Errors.Num() > 0)
	{
		OutError = FString::Join(Errors, TEXT("\n"));
		return false;
	}
	return true;
}
