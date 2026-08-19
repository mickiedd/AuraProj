// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindCivilianDestination.generated.h"

/** Picks a reachable ground destination for the owning Civilian. */
UCLASS()
class AURA_API UBTTask_FindCivilianDestination : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindCivilianDestination();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Civilian|Wander", meta = (ClampMin = "50.0"))
	float SearchRadius = 300.f;
};
