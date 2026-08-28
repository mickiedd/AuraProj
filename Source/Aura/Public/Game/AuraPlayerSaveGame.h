// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Game/LoadScreenSaveGame.h"
#include "Economy/AuraEconomyTypes.h"
#include "AuraPlayerSaveGame.generated.h"

/** One versioned record for one authenticated profile. */
UCLASS()
class AURA_API UAuraPlayerSaveGame : public ULoadScreenSaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY()
	int32 SaveSchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int32 RecordGeneration = 0;

	UPROPERTY()
	FString IdentityProvider;

	UPROPERTY()
	FString IdentityValue;

	UPROPERTY()
	FName CurrencyId = NAME_None;

	UPROPERTY()
	int64 CurrencyBalance = 0;

	UPROPERTY()
	uint32 CurrencyRevision = 0;

	UPROPERTY()
	TArray<FAuraInventorySlot> InventorySlots;

	UPROPERTY()
	uint32 InventoryRevision = 0;

	UPROPERTY()
	bool bMigrationCompleted = false;
};
