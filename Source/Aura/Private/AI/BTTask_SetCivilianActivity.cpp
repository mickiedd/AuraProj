// Copyright Druid Mechanics

#include "AI/BTTask_SetCivilianActivity.h"

#include "AIController.h"
#include "Character/AuraCivilian.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_SetCivilianActivity::UBTTask_SetCivilianActivity()
{
	NodeName = TEXT("Set Civilian Activity");
}

EBTNodeResult::Type UBTTask_SetCivilianActivity::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAuraCivilian* Civilian = OwnerComp.GetAIOwner() ? Cast<AAuraCivilian>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	if (!Civilian || !Civilian->HasAuthority() || !Civilian->IsCombatAlive()) return EBTNodeResult::Failed;
	Civilian->SetCivilianActivity(Activity);
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsEnum(TEXT("Activity"), static_cast<uint8>(Activity));
	}
	return EBTNodeResult::Succeeded;
}
