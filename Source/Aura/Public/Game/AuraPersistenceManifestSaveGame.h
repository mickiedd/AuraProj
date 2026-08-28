// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/AuraSaveTypes.h"
#include "AuraPersistenceManifestSaveGame.generated.h"

/** Gameplay-free commit marker written last for a persistence checkpoint. */
UCLASS()
class AURA_API UAuraPersistenceManifestSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY()
	int32 SaveSchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int32 Generation = 0;

	UPROPERTY()
	FString WorldPersistenceId;

	UPROPERTY()
	FAuraPersistenceManifestRecord WorldRecord;

	UPROPERTY()
	TArray<FAuraPersistenceManifestRecord> PlayerRecords;

	UPROPERTY()
	FString Checksum;
};
