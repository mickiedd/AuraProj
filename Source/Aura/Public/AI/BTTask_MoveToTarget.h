// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveToTarget.generated.h"

/**
 * Custom MoveTo task with full diagnostic logging.
 * Reads the target actor from a Blackboard object key, issues a MoveToActor
 * request through the AIController, and reports result/failure reason to the log.
 */
UCLASS()
class AURA_API UBTTask_MoveToTarget : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MoveToTarget();

	/** Distance at which the move is considered complete. */
	UPROPERTY(EditAnywhere, Category = "Move", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 50.f;

	/** If true the pawn stops as soon as it enters AcceptanceRadius of the target. */
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bStopOnOverlap = true;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 SenderID, bool bSuccess) override;
	virtual FString GetStaticDescription() const override;
};
