// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Economy/AuraEconomyTypes.h"
#include "World/AuraPopulationTypes.h"
#include "AuraSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct AURA_API FAuraPersistedOfferStock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	FName OfferId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	int64 CurrentStock = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPersistedMerchantStock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	FName PopulationMemberId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	FName MerchantDefinitionId = NAME_None;

	UPROPERTY()
	uint32 StockRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Merchant")
	TArray<FAuraPersistedOfferStock> Offers;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPersistenceManifestRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Manifest")
	FString RecordSlot;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Manifest")
	int32 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Manifest")
	FString Checksum;

	/** Canonical profile key for player records; empty only for the shared world record. */
	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Manifest")
	FString IdentityKey;
};
