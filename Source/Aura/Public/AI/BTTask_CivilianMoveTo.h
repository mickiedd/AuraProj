// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BTTask_CivilianMoveTo.generated.h"

/** MoveTo specialization bound to the Civilian Destination blackboard key. */
UCLASS()
class AURA_API UBTTask_CivilianMoveTo : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_CivilianMoveTo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
