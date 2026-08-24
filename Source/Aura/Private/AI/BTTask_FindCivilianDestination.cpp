// Copyright Druid Mechanics

#include "AI/BTTask_FindCivilianDestination.h"

#include "AIController.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraCivilian.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/AuraCharacterBase.h"
#include "Game/AuraGameModeBase.h"
#include "NavigationSystem.h"
#include "World/AuraCivilianActivityMarker.h"
#include "World/AuraCivilianObservationMarker.h"
#include "World/AuraCivilianShelterMarker.h"
#include "World/AuraCivilianWorkMarker.h"
#include "World/AuraPopulationManager.h"
#include "World/AuraPopulationTypes.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace AuraCivilianDestinationPrivate
{
	bool IsCompatible(const AAuraCivilianActivityMarker* Marker, const FAuraPopulationMemberState& MemberState)
	{
		return Marker && (Marker->GetZoneId().IsNone() || Marker->GetZoneId() == MemberState.ZoneId);
	}
}

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

	AAuraGameModeBase* GameMode = Civilian->GetWorld() ? Civilian->GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr;
	UAuraPopulationManager* PopulationManager = GameMode ? GameMode->GetPopulationManagerMutable() : nullptr;
	const FAuraPopulationMemberState& MemberState = Civilian->GetPopulationMemberState();
	const FAuraCivilianWorkProfile* Profile = PopulationManager ? PopulationManager->FindWorkProfile(MemberState.WorkProfileId) : nullptr;
	if (!PopulationManager || !Profile || !MemberState.IsValid())
	{
		return EBTNodeResult::Failed;
	}
	PopulationManager->ReleaseAllActivityMarkerReservations(MemberState.PopulationMemberId);
	Blackboard->ClearValue(TEXT("WorkMarker"));
	Blackboard->ClearValue(TEXT("ObservationMarker"));
	Blackboard->ClearValue(TEXT("ShelterMarker"));

	const AActor* ThreatActor = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("ThreatActor")));
	TArray<AAuraCivilianActivityMarker*> Markers;
	PopulationManager->GetActivityMarkers(Markers);

	if (IsValid(ThreatActor))
	{
		UNavigationSystemV1* ThreatNavigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Civilian->GetWorld());
		for (AAuraCivilianActivityMarker* Marker : Markers)
		{
			AAuraCivilianShelterMarker* Shelter = Cast<AAuraCivilianShelterMarker>(Marker);
			double ShelterPathLength = 0.0;
			const bool bShelterReachable = ThreatNavigation && Shelter
				&& UNavigationSystemV1::GetPathLength(Civilian->GetWorld(), Civilian->GetActorLocation(), Shelter->GetActorLocation(), ShelterPathLength)
					== ENavigationQueryResult::Success;
			if (!Shelter || !AuraCivilianDestinationPrivate::IsCompatible(Shelter, MemberState)
				|| !Shelter->CanAccept(MemberState.WorkProfileId, Profile->ShelterMarkerTags)
				|| !bShelterReachable
				|| !PopulationManager->TryReserveActivityMarker(Shelter->GetMarkerId(), MemberState.PopulationMemberId))
			{
				continue;
			}
			Blackboard->SetValueAsObject(TEXT("ShelterMarker"), Shelter);
			Blackboard->SetValueAsVector(TEXT("Destination"), Shelter->GetActorLocation());
			Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), true);
			Civilian->SetCivilianActivity(EAuraCivilianActivity::Shelter);
			if (Civilian->GetCharacterMovement()) Civilian->GetCharacterMovement()->MaxWalkSpeed = Profile->FleeMovementSpeed;
			return EBTNodeResult::Succeeded;
		}

		const FVector AwayDirection = (Civilian->GetActorLocation() - ThreatActor->GetActorLocation()).GetSafeNormal2D();
		const FVector DesiredFleeLocation = Civilian->GetActorLocation() + (AwayDirection.IsNearlyZero() ? FVector::ForwardVector : AwayDirection) * Profile->FleeDistance;
		FNavLocation FleeDestination;
		if (ThreatNavigation && ThreatNavigation->ProjectPointToNavigation(DesiredFleeLocation, FleeDestination, FVector(250.f, 250.f, 400.f)))
		{
			Blackboard->SetValueAsVector(TEXT("Destination"), FleeDestination.Location);
			Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), true);
			Civilian->SetCivilianActivity(EAuraCivilianActivity::Flee);
			if (Civilian->GetCharacterMovement()) Civilian->GetCharacterMovement()->MaxWalkSpeed = Profile->FleeMovementSpeed;
			return EBTNodeResult::Succeeded;
		}
		Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), false);
		return EBTNodeResult::Failed;
	}

	for (AAuraCivilianActivityMarker* Marker : Markers)
	{
		AAuraCivilianWorkMarker* WorkMarker = Cast<AAuraCivilianWorkMarker>(Marker);
		if (!WorkMarker || !AuraCivilianDestinationPrivate::IsCompatible(WorkMarker, MemberState)
			|| !WorkMarker->AcceptsWorkProfile(MemberState.WorkProfileId)
			|| !WorkMarker->CanAccept(MemberState.WorkProfileId, Profile->WorkMarkerTags)
			|| !PopulationManager->TryReserveActivityMarker(WorkMarker->GetMarkerId(), MemberState.PopulationMemberId))
		{
			continue;
		}
		Blackboard->SetValueAsObject(TEXT("WorkMarker"), WorkMarker);
		Blackboard->SetValueAsVector(TEXT("Destination"), WorkMarker->GetActorLocation());
		Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), true);
		Civilian->SetCivilianActivity(EAuraCivilianActivity::Work);
		if (Civilian->GetCharacterMovement()) Civilian->GetCharacterMovement()->MaxWalkSpeed = Profile->MovementSpeed;
		return EBTNodeResult::Succeeded;
	}

	for (AAuraCivilianActivityMarker* Marker : Markers)
	{
		AAuraCivilianObservationMarker* ObservationMarker = Cast<AAuraCivilianObservationMarker>(Marker);
		if (!ObservationMarker || !AuraCivilianDestinationPrivate::IsCompatible(ObservationMarker, MemberState)
			|| !ObservationMarker->CanAccept(MemberState.WorkProfileId, Profile->ObservationMarkerTags)
			|| !PopulationManager->TryReserveActivityMarker(ObservationMarker->GetMarkerId(), MemberState.PopulationMemberId))
		{
			continue;
		}
		Blackboard->SetValueAsObject(TEXT("ObservationMarker"), ObservationMarker);
		Blackboard->SetValueAsVector(TEXT("Destination"), ObservationMarker->GetActorLocation());
		Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), true);
		Civilian->SetCivilianActivity(EAuraCivilianActivity::Observe);
		if (Civilian->GetCharacterMovement()) Civilian->GetCharacterMovement()->MaxWalkSpeed = Profile->MovementSpeed;
		return EBTNodeResult::Succeeded;
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
	if (Civilian->GetCharacterMovement()) Civilian->GetCharacterMovement()->MaxWalkSpeed = Profile->MovementSpeed;
	UE_LOG(LogAura, Log, TEXT("[CivilianAI][BT] %s destination=%s activity=Wander."),
		*GetNameSafe(Civilian), *Destination.Location.ToCompactString());
	return EBTNodeResult::Succeeded;
}
