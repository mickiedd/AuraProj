// Copyright Druid Mechanics

#include "AI/BTService_UpdateCivilianContext.h"

#include "AIController.h"
#include "Character/AuraCivilian.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_UpdateCivilianContext::UBTService_UpdateCivilianContext()
{
	NodeName = TEXT("Update Civilian Context");
	Interval = 0.5f;
}

void UBTService_UpdateCivilianContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AAuraCivilian* Civilian = OwnerComp.GetAIOwner() ? Cast<AAuraCivilian>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Civilian || !Blackboard || !Civilian->HasAuthority() || !Civilian->IsCombatAlive()) return;
	if (!IsValid(Blackboard->GetValueAsObject(TEXT("ThreatActor"))))
	{
		Blackboard->ClearValue(TEXT("ThreatActor"));
		Blackboard->SetValueAsFloat(TEXT("ThreatDistance"), TNumericLimits<float>::Max());
	}
}
