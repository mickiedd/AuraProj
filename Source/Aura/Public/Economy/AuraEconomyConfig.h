// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Economy/AuraEconomyTypes.h"

struct FAuraPopulationSpawnRow;

class AURA_API FAuraEconomyConfigLoader
{
public:
	static bool LoadFromProjectFiles(const TArray<FAuraPopulationSpawnRow>& PopulationRows,
		FAuraEconomySnapshot& OutSnapshot, FString& OutError);
	static bool ParseCanonicalNonNegativeInt64(const FString& Text, bool bRequirePositive, int64& OutValue, FString& OutError);
	static FName NormalizeId(const FString& RawId);
};
