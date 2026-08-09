// Copyright Druid Mechanics


#include "AI/BTService_FindNearestPlayer.h"
#include "AIController.h"
#include "BehaviorTree/BTFunctionLibrary.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatRules.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"

void UBTService_FindNearestPlayer::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (!IsValid(AIOwner))
	{
		return;
	}

	APawn* OwningPawn = AIOwner->GetPawn();
	if (!IsValid(OwningPawn))
	{
		return;
	}

	AActor* ClosestActor = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();

	TArray<AActor*> CandidateTargets;
	TArray<AActor*> AllPawns;
	UGameplayStatics::GetAllActorsOfClass(OwningPawn, APawn::StaticClass(), AllPawns);
	const FGameplayTag PlayerFaction = FAuraGameplayTags::Get().Faction_Player;
	for (AActor* Actor : AllPawns)
	{
		APawn* CandidatePawn = Cast<APawn>(Actor);
		if (!IsValid(CandidatePawn) || CandidatePawn == OwningPawn)
		{
			continue;
		}

		const UAuraCombatIdentityComponent* IdentityComponent = UAuraCombatIdentityComponent::FindForActor(CandidatePawn);
		if (IdentityComponent
			&& IdentityComponent->HasValidIdentity()
			&& IdentityComponent->GetIdentity().FactionTag.MatchesTagExact(PlayerFaction))
		{
			FAuraCombatRuleContext RuleContext;
			RuleContext.QueryPurpose = EAuraCombatQueryPurpose::CombatTargeting;
			RuleContext.TrustedWorldContext = OwningPawn;
			RuleContext.SourceActor = OwningPawn;
			RuleContext.TargetActor = CandidatePawn;
			const FAuraCombatRuleResult RuleResult = FAuraCombatRules::CanCombatTarget(OwningPawn, CandidatePawn, RuleContext);
			if (RuleResult.bCanCombatTarget)
			{
				CandidateTargets.Add(CandidatePawn);
			}
		}
	}

	for (AActor* Actor : CandidateTargets)
	{
		if (IsValid(Actor))
		{
			const float Distance = OwningPawn->GetDistanceTo(Actor);
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestActor = Actor;
			}
		}
	}

	UBTFunctionLibrary::SetBlackboardValueAsObject(this, TargetToFollowSelector, ClosestActor);
	UBTFunctionLibrary::SetBlackboardValueAsFloat(this, DistanceToTargetSelector, ClosestDistance);

	if (UWorld* World = OwningPawn->GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastDebugLogTime >= 10.0f)
		{
			LastDebugLogTime = Now;

			// Read all critical blackboard values
			UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
			UObject* BBTarget         = BB ? BB->GetValueAsObject(FName("TargetToFollow")) : nullptr;
			const float BBDistance    = BB ? BB->GetValueAsFloat(FName("DistanceToTarget")) : -1.f;
			const bool bRanged        = BB && BB->GetValueAsBool(FName("RangedAttacker"));
			const bool bHitReacting   = BB && BB->GetValueAsBool(FName("HitReacting"));
			const bool bDead          = BB && BB->GetValueAsBool(FName("Dead"));
			const bool bStunned       = BB && BB->GetValueAsBool(FName("Stunned"));
			const FVector MoveToLoc   = BB ? BB->GetValueAsVector(FName("MoveToLocation")) : FVector::ZeroVector;

			UE_LOG(LogAura, Log, TEXT("[EnemyAI][FindNearestPlayer] Pawn=%s | Candidates=%d Closest=%s Distance=%.1f | BB: Target=%s BBDist=%.1f Ranged=%d HitReacting=%d Dead=%d Stunned=%d MoveToLoc=%s"),
				*GetNameSafe(OwningPawn),
				CandidateTargets.Num(),
				*GetNameSafe(ClosestActor),
				ClosestDistance,
				*GetNameSafe(BBTarget),
				BBDistance,
				bRanged, bHitReacting, bDead, bStunned,
				*MoveToLoc.ToCompactString());
		}
	}
}
