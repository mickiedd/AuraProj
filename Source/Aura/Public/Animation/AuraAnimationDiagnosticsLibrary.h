// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AuraAnimationDiagnosticsLibrary.generated.h"

class APawn;
class UCharacterMovementComponent;

/** Runtime probes used to isolate Civilian locomotion failures without logging every frame. */
UCLASS()
class AURA_API UAuraAnimationDiagnosticsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Aura|Animation|Diagnostics")
	static void LogCivilianAnimationState(
		APawn* PawnOwner,
		UCharacterMovementComponent* CachedMovement,
		float BlueprintGroundSpeed);
};
