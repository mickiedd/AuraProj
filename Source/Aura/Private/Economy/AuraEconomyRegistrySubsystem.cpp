// Copyright Druid Mechanics

#include "Economy/AuraEconomyRegistrySubsystem.h"

#include "Aura/AuraLogChannels.h"
#include "Economy/AuraEconomyConfig.h"
#include "Engine/World.h"

bool UAuraEconomyRegistrySubsystem::LoadAndPublishAuthorityRegistry(const UObject* WorldContext,
	const TArray<FAuraPopulationSpawnRow>& PopulationRows, FString& OutError)
{
	if (!IsValid(WorldContext) || !WorldContext->GetWorld() || WorldContext->GetWorld()->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Economy registry publication requires an authority world.");
		return false;
	}
	FAuraEconomySnapshot Candidate;
	if (!FAuraEconomyConfigLoader::LoadFromProjectFiles(PopulationRows, Candidate, OutError))
	{
		// Atomic reload semantics retain the prior valid snapshot. The game-mode
		// readiness latch separately prevents that snapshot from satisfying a new
		// map's startup gate after a failed load.
		return false;
	}
	Candidate.Generation = PublishedSnapshot.IsValid() ? PublishedSnapshot->Generation + 1 : 1;
	PublishedSnapshot = MakeShared<const FAuraEconomySnapshot>(MoveTemp(Candidate));
	UE_LOG(LogAura, Display, TEXT("[Economy][Registry] Published immutable generation=%d schema=%d items=%d offers=%d merchants=%d."),
		PublishedSnapshot->Generation, PublishedSnapshot->SchemaVersion, PublishedSnapshot->Items.Num(),
		PublishedSnapshot->Offers.Num(), PublishedSnapshot->Merchants.Num());
	return true;
}

const FAuraItemDefinition* UAuraEconomyRegistrySubsystem::FindItem(FName ItemId) const
{
	return PublishedSnapshot.IsValid() ? PublishedSnapshot->Items.Find(ItemId) : nullptr;
}

const FAuraOfferDefinition* UAuraEconomyRegistrySubsystem::FindOffer(FName OfferId) const
{
	return PublishedSnapshot.IsValid() ? PublishedSnapshot->Offers.Find(OfferId) : nullptr;
}

const FAuraMerchantDefinition* UAuraEconomyRegistrySubsystem::FindMerchant(FName MerchantId) const
{
	return PublishedSnapshot.IsValid() ? PublishedSnapshot->Merchants.Find(MerchantId) : nullptr;
}
