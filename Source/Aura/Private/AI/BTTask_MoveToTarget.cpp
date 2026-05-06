// Copyright Druid Mechanics

#include "AI/BTTask_MoveToTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Aura/AuraLogChannels.h"

UBTTask_MoveToTarget::UBTTask_MoveToTarget()
{
	NodeName = TEXT("Move To Target (Debug)");

	// Accept any object key so the designer can pick TargetToFollow in the BT editor
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToTarget, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!IsValid(Controller))
	{
		UE_LOG(LogAura, Warning, TEXT("[BTTask_MoveToTarget] FAILED — no AIController"));
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BB))
	{
		UE_LOG(LogAura, Warning, TEXT("[BTTask_MoveToTarget] Pawn=%s — FAILED: no BlackboardComponent"), *GetNameSafe(Pawn));
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(BlackboardKey.SelectedKeyName));
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogAura, Warning, TEXT("[BTTask_MoveToTarget] Pawn=%s — FAILED: TargetToFollow is null/invalid (BB key=%s)"),
			*GetNameSafe(Pawn),
			*BlackboardKey.SelectedKeyName.ToString());
		return EBTNodeResult::Failed;
	}

	const float CurrentDist = Pawn ? Pawn->GetDistanceTo(TargetActor) : -1.f;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller);
	FNavLocation PawnNavLocation;
	FNavLocation TargetNavLocation;
	const bool bPawnOnNav = NavSys && Pawn && NavSys->ProjectPointToNavigation(Pawn->GetActorLocation(), PawnNavLocation, FVector(200.f, 200.f, 500.f));
	const bool bTargetOnNav = NavSys && NavSys->ProjectPointToNavigation(TargetActor->GetActorLocation(), TargetNavLocation, FVector(200.f, 200.f, 500.f));

	bool bPathExists = false;
	if (NavSys && Pawn)
	{
		UNavigationPath* TestPath = NavSys->FindPathToActorSynchronously(
			Controller,
			Pawn->GetActorLocation(),
			TargetActor,
			AcceptanceRadius,
			Controller->GetPawn());
		bPathExists = IsValid(TestPath) && TestPath->IsValid() && !TestPath->PathPoints.IsEmpty();
	}

	UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] Pawn=%s  Target=%s  Distance=%.1f  AcceptRadius=%.1f — Requesting move..."),
		*GetNameSafe(Pawn),
		*GetNameSafe(TargetActor),
		CurrentDist,
		AcceptanceRadius);
	UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] NavDiag PawnOnNav=%d TargetOnNav=%d PathExists=%d PawnLoc=%s TargetLoc=%s PawnNav=%s TargetNav=%s"),
		bPawnOnNav,
		bTargetOnNav,
		bPathExists,
		Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("None"),
		*TargetActor->GetActorLocation().ToCompactString(),
		bPawnOnNav ? *PawnNavLocation.Location.ToCompactString() : TEXT("None"),
		bTargetOnNav ? *TargetNavLocation.Location.ToCompactString() : TEXT("None"));

	FAIMoveRequest MoveRequest(TargetActor);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(bStopOnOverlap);
	MoveRequest.SetReachTestIncludesGoalRadius(bStopOnOverlap);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);

	const FPathFollowingRequestResult Result = Controller->MoveTo(MoveRequest);

	switch (Result.Code)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
	{
		const uint32 MoveId = Result.MoveId.GetID();
		UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] Pawn=%s — MoveRequest ACCEPTED (MoveID=%d), waiting for finish..."),
			*GetNameSafe(Pawn), static_cast<int32>(MoveId));
		WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, static_cast<int32>(MoveId));
		return EBTNodeResult::InProgress;
	}

	case EPathFollowingRequestResult::AlreadyAtGoal:
		UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] Pawn=%s — AlreadyAtGoal (Distance=%.1f <= AcceptRadius=%.1f)"),
			*GetNameSafe(Pawn), CurrentDist, AcceptanceRadius);
		return EBTNodeResult::Succeeded;

	case EPathFollowingRequestResult::Failed:
	default:
		UE_LOG(LogAura, Warning, TEXT("[BTTask_MoveToTarget] Pawn=%s — MoveRequest FAILED (Controller valid=%d NavComponent valid=%d PawnOnNav=%d TargetOnNav=%d PathExists=%d)"),
			*GetNameSafe(Pawn),
			IsValid(Controller),
			IsValid(Controller->GetPathFollowingComponent()),
			bPawnOnNav,
			bTargetOnNav,
			bPathExists);
		return EBTNodeResult::Failed;
	}
}

EBTNodeResult::Type UBTTask_MoveToTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (IsValid(Controller))
	{
		Controller->StopMovement();
		UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] Pawn=%s — Move ABORTED"), *GetNameSafe(Controller->GetPawn()));
	}
	return EBTNodeResult::Aborted;
}

void UBTTask_MoveToTarget::OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 SenderID, bool bSuccess)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;

	if (Message == UBrainComponent::AIMessage_MoveFinished)
	{
		UE_LOG(LogAura, Log, TEXT("[BTTask_MoveToTarget] Pawn=%s — Move FINISHED (MoveID=%d  Success=%d)"),
			*GetNameSafe(Pawn), SenderID, bSuccess);

		FinishLatentTask(OwnerComp, bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
	else
	{
		Super::OnMessage(OwnerComp, NodeMemory, Message, SenderID, bSuccess);
	}
}

FString UBTTask_MoveToTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Move to: %s\nAcceptRadius: %.1f"), *BlackboardKey.SelectedKeyName.ToString(), AcceptanceRadius);
}
