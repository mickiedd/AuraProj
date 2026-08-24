// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/AuraEconomyTypes.h"
#include "AuraEconomyRegistrySubsystem.generated.h"

struct FAuraPopulationSpawnRow;

UCLASS()
class AURA_API UAuraEconomyRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool LoadAndPublishAuthorityRegistry(const UObject* WorldContext, const TArray<FAuraPopulationSpawnRow>& PopulationRows, FString& OutError);
	bool IsReady() const { return PublishedSnapshot.IsValid(); }
	int32 GetGeneration() const { return PublishedSnapshot.IsValid() ? PublishedSnapshot->Generation : 0; }
	TSharedPtr<const FAuraEconomySnapshot> GetSnapshot() const { return PublishedSnapshot; }
	const FAuraItemDefinition* FindItem(FName ItemId) const;
	const FAuraOfferDefinition* FindOffer(FName OfferId) const;
	const FAuraMerchantDefinition* FindMerchant(FName MerchantId) const;

private:
	TSharedPtr<const FAuraEconomySnapshot> PublishedSnapshot;
};
