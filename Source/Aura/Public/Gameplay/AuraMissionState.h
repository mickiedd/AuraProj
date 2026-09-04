// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Gameplay/AuraMissionTypes.h"
#include "AuraMissionState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraMissionStateChanged, const FAuraMissionPublicSnapshot&);

/** Replicated public mission snapshot. Private loadout/profile data stays on owner components. */
UCLASS()
class AURA_API AAuraMissionState : public AInfo
{
	GENERATED_BODY()

public:
	AAuraMissionState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FAuraMissionPublicSnapshot& GetPublicSnapshot() const { return ReplicatedPublicSnapshot; }
	/** Authority-only snapshot publication; the actor never accepts client state. */
	bool PublishRunState(const FAuraMissionRunState& InState);

	FAuraMissionStateChanged OnStateChanged;

protected:
	UFUNCTION()
	void OnRep_ReplicatedPublicSnapshot();

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedPublicSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mission")
	FAuraMissionPublicSnapshot ReplicatedPublicSnapshot;
};
