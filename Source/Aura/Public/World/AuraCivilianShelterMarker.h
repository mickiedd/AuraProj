// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraCivilianActivityMarker.h"
#include "AuraCivilianShelterMarker.generated.h"

UCLASS()
class AURA_API AAuraCivilianShelterMarker : public AAuraCivilianActivityMarker
{
	GENERATED_BODY()

public:
	float GetProtectionRadius() const { return ProtectionRadius; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker|Shelter", meta = (ClampMin = "0.0"))
	float ProtectionRadius = 500.f;
};
