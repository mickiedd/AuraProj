// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/AuraCombatStatusTypes.h"
#include "AuraCombatStatusComponent.generated.h"

/** Authority-only status/interrupt seam; it intentionally does not apply GAS effects yet. */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraCombatStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraCombatStatusComponent();

	EAuraCombatStatusResult TryApplyStatus(const FAuraCombatStatusKey& Key, FName StackingPolicy,
		FName InterruptClassification, double AuthorityNow, double DurationSeconds, FString& OutError);
	EAuraCombatStatusResult TryRemoveStatus(const FAuraCombatStatusKey& Key, EAuraCombatStatusRemovalReason Reason,
		FString& OutError);
	bool TryBeginInterruptibleChannel(const FAuraInterruptChannelKey& Key, double AuthorityNow,
		double DurationSeconds, FString& OutError);
	EAuraInterruptResult TryInterruptChannel(const FAuraInterruptChannelKey& Key, int32 InterruptSequence,
		double AuthorityNow, FString& OutError);
	int32 PruneExpired(double AuthorityNow);

	const FAuraCombatStatusLedger& GetLedger() const { return Ledger; }

private:
	bool HasAuthorityContext(FString& OutError) const;

	UPROPERTY(Transient)
	FAuraCombatStatusLedger Ledger;
};
