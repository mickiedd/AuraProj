// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/AuraEvadePolicy.h"
#include "AuraEvadeComponent.generated.h"

/**
 * Server-only host for evade admission. It does not bind an action or move the
 * owner; a future runtime adapter must consume the acceptance and perform the
 * authoritative swept movement after applying its cancellation plan.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraEvadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraEvadeComponent();

	bool InitializeAuthorityState(const FGuid& InRunId, int32 InEpoch, FName InOwnerId, int32 InOwnerLifeGeneration);
	EAuraEvadeResult TryAcceptRequest(const FAuraEvadeRequest& Request, const FAuraEvadeOwnerSnapshot& OwnerSnapshot,
		double AuthorityNow, FAuraEvadeAcceptance& OutAcceptance, FString& OutError);
	EAuraEvadeSweepResult ResolveSweep(const FAuraEvadeRequestKey& RequestKey, double AuthorityNow,
		float TravelDistanceUnits, bool bBlocked, FString& OutError);

	const FAuraEvadeState& GetState() const { return State; }

private:
	UPROPERTY(Transient)
	FAuraEvadeState State;
};
