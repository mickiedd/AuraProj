// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CivilianAlive.generated.h"

UCLASS()
class AURA_API UBTDecorator_CivilianAlive : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CivilianAlive();
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
