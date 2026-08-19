// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraPopulationTypes.h"
#include "AuraPopulationSpawnDefinition.generated.h"

class FJsonObject;

/** Runtime, versioned loader for the Day 09 population fixtures. */
UCLASS(Transient)
class AURA_API UAuraPopulationSpawnDefinition : public UObject
{
	GENERATED_BODY()

public:
	bool LoadDefinitions(FString& OutError);

	int32 GetSchemaVersion() const { return SchemaVersion; }
	const TArray<FAuraPopulationSpawnRow>& GetPopulationRows() const { return PopulationRows; }
	const TArray<FAuraCivilianWorkProfile>& GetWorkProfiles() const { return WorkProfiles; }
	const TMap<FName, FString>& GetLevelMapPaths() const { return LevelMapPaths; }

	const FAuraCivilianWorkProfile* FindWorkProfile(FName WorkProfileId) const;

	static FString GetPopulationTablePath();
	static FString GetWorkProfileTablePath();

private:
	bool LoadJsonObject(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const;
	bool ParseWorkProfiles(const TSharedPtr<FJsonObject>& Root, TArray<FString>& OutErrors);
	bool ParsePopulationRows(const TSharedPtr<FJsonObject>& Root, TArray<FString>& OutErrors);
	bool ParseLevels(TArray<FString>& OutErrors);

	UPROPERTY(Transient)
	int32 SchemaVersion = 0;

	UPROPERTY(Transient)
	TArray<FAuraPopulationSpawnRow> PopulationRows;

	UPROPERTY(Transient)
	TArray<FAuraCivilianWorkProfile> WorkProfiles;

	TMap<FName, FString> LevelMapPaths;
};
