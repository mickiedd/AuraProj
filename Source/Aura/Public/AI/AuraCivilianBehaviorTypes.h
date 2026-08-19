// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraCivilianBehaviorTypes.generated.h"

/** Server-authoritative activity exposed to clients for UI/debug presentation. */
UENUM(BlueprintType)
enum class EAuraCivilianActivity : uint8
{
	Idle,
	Wander,
	Work,
	Observe,
	Flee,
	Shelter,
};
