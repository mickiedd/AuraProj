// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/AuraSaveTypes.h"
#include "AuraWorldSaveGame.generated.h"

/** One versioned record for one authority-owned campaign/world identity. */
UCLASS()
class AURA_API UAuraWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY()
	int32 SaveSchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int32 RecordGeneration = 0;

	UPROPERTY()
	FString WorldPersistenceId;

	UPROPERTY()
	FString MapPackage;

	UPROPERTY()
	TArray<FAuraPopulationSlotSnapshot> PopulationSlots;

	UPROPERTY()
	TArray<FAuraPersistedMerchantStock> Merchants;
};
