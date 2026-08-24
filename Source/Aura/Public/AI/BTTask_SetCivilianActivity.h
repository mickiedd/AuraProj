// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AI/AuraCivilianBehaviorTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetCivilianActivity.generated.h"

UCLASS()
class AURA_API UBTTask_SetCivilianActivity : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetCivilianActivity();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Civilian")
	EAuraCivilianActivity Activity = EAuraCivilianActivity::Idle;
};
