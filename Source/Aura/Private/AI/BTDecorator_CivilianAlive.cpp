// Copyright Druid Mechanics

#include "AI/BTDecorator_CivilianAlive.h"

#include "AIController.h"
#include "Character/AuraCivilian.h"

UBTDecorator_CivilianAlive::UBTDecorator_CivilianAlive()
{
	NodeName = TEXT("Civilian Alive");
	bNotifyBecomeRelevant = true;
}

bool UBTDecorator_CivilianAlive::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAuraCivilian* Civilian = OwnerComp.GetAIOwner() ? Cast<AAuraCivilian>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	return Civilian && Civilian->HasAuthority() && Civilian->IsCombatAlive();
}
