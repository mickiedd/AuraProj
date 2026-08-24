// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraCivilianActivityMarker.h"
#include "AuraCivilianWorkMarker.generated.h"

UCLASS()
class AURA_API AAuraCivilianWorkMarker : public AAuraCivilianActivityMarker
{
	GENERATED_BODY()

public:
	bool AcceptsWorkProfile(FName WorkProfileId) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker|Work")
	TArray<FName> AllowedWorkProfileIds;
};
