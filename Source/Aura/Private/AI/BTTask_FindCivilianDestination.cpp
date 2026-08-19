// Copyright Druid Mechanics

#include "AI/BTTask_FindCivilianDestination.h"

#include "AIController.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraCivilian.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_FindCivilianDestination::UBTTask_FindCivilianDestination()
{
	NodeName = TEXT("Find Civilian Destination");
}

EBTNodeResult::Type UBTTask_FindCivilianDestination::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	AAuraCivilian* Civilian = Controller ? Cast<AAuraCivilian>(Controller->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Civilian || !Blackboard || !Civilian->IsCombatAlive())
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Civilian->GetWorld());
	if (!NavigationSystem)
	{
		UE_LOG(LogAura, Warning, TEXT("[CivilianAI][BT] No navigation system for %s; destination selection failed."),
			*GetNameSafe(Civilian));
		return EBTNodeResult::Failed;
	}

	FNavLocation NavOrigin;
	if (!NavigationSystem->ProjectPointToNavigation(Civilian->GetActorLocation(), NavOrigin, FVector(150.f, 150.f, 300.f)))
	{
		UE_LOG(LogAura, Warning, TEXT("[CivilianAI][BT] Civilian %s is not on navigation data at %s."),
			*GetNameSafe(Civilian), *Civilian->GetActorLocation().ToCompactString());
		return EBTNodeResult::Failed;
	}

	FNavLocation Destination;
	if (!NavigationSystem->GetRandomReachablePointInRadius(NavOrigin.Location, SearchRadius, Destination))
	{
		UE_LOG(LogAura, Warning, TEXT("[CivilianAI][BT] No reachable destination for %s origin=%s radius=%.1f."),
			*GetNameSafe(Civilian), *NavOrigin.Location.ToCompactString(), SearchRadius);
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("Destination"), Destination.Location);
	Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), true);
	Civilian->SetCivilianActivity(EAuraCivilianActivity::Wander);
	UE_LOG(LogAura, Log, TEXT("[CivilianAI][BT] %s destination=%s activity=Wander."),
		*GetNameSafe(Civilian), *Destination.Location.ToCompactString());
	return EBTNodeResult::Succeeded;
}
