// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateCivilianContext.generated.h"

UCLASS()
class AURA_API UBTService_UpdateCivilianContext : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateCivilianContext();
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
