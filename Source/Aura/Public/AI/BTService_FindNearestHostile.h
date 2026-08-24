// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_FindNearestHostile.generated.h"

/** Day 10 replacement for the old FindNearestPlayer service. */
UCLASS()
class AURA_API UBTService_FindNearestHostile : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_FindNearestHostile();
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FBlackboardKeySelector TargetToFollowSelector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FBlackboardKeySelector DistanceToTargetSelector;

	float LastDebugLogTime = -1000.f;
};
