// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraRunLoadoutComponent.generated.h"

/** Transient owner-only state for one mission run; it is never part of the persistent profile. */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraRunLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraRunLoadoutComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool BeginRun(const FGuid& RunId, int32 Epoch);
	bool AddAugment(FName AugmentId);
	bool ConsumeEmergencyRefill();
	bool EndRun();

	bool HasActiveRun() const { return ActiveRunId.IsValid() && ActiveRunEpoch > 0; }
	const FGuid& GetActiveRunId() const { return ActiveRunId; }
	int32 GetActiveRunEpoch() const { return ActiveRunEpoch; }
	const TArray<FName>& GetSelectedAugments() const { return SelectedAugments; }
	int32 GetEmergencyRefillCharges() const { return EmergencyRefillCharges; }

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mission|Run Loadout")
	FGuid ActiveRunId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mission|Run Loadout")
	int32 ActiveRunEpoch = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mission|Run Loadout")
	TArray<FName> SelectedAugments;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mission|Run Loadout")
	int32 EmergencyRefillCharges = 0;
};
