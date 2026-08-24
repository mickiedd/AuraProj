// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraCivilianActivityMarker.h"
#include "AuraCivilianObservationMarker.generated.h"

UCLASS()
class AURA_API AAuraCivilianObservationMarker : public AAuraCivilianActivityMarker
{
	GENERATED_BODY()

public:
	float GetObservationDuration() const { return ObservationDuration; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker|Observation", meta = (ClampMin = "0.1"))
	float ObservationDuration = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker|Observation")
	FVector FacingDirection = FVector::ForwardVector;
};
