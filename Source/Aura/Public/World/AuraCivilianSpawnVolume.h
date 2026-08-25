// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraCivilianSpawnVolume.generated.h"

class UBoxComponent;

/** Candidate-transform provider. Counts, slots and actor lifecycle remain manager-owned. */
UCLASS()
class AURA_API AAuraCivilianSpawnVolume : public AActor
{
	GENERATED_BODY()

public:
	AAuraCivilianSpawnVolume();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FName GetSpawnVolumeId() const { return SpawnVolumeId; }
	void SetSpawnVolumeIdForRuntime(FName InId);
	bool GetCandidateTransform(int32 AttemptIndex, FTransform& OutTransform) const;

protected:
	void ApplyCandidateExtents();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Population")
	FName SpawnVolumeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population")
	FVector CandidateExtents = FVector(500.f, 500.f, 100.f);
};
