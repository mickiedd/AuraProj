// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_FindNearestThreat.generated.h"

/** Finds an actor that is currently allowed to damage the owning Civilian. */
UCLASS()
class AURA_API UBTService_FindNearestThreat : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_FindNearestThreat();
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Civilian|Threat", meta = (ClampMin = "0.1"))
	float ServiceInterval = 0.5f;
};
