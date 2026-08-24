// Copyright Druid Mechanics

#include "AI/BTService_FindNearestHostile.h"

#include "AIController.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "BehaviorTree/BTFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/AuraCharacterBase.h"
#include "Combat/AuraCombatRules.h"
#include "Battle/AuraBattleDirector.h"
#include "EngineUtils.h"
#include "Game/AuraGameModeBase.h"

UBTService_FindNearestHostile::UBTService_FindNearestHostile()
{
	NodeName = TEXT("Find Nearest Hostile");
	Interval = 0.5f;
	RandomDeviation = 0.1f;
	TargetToFollowSelector.SelectedKeyName = TEXT("TargetToFollow");
	DistanceToTargetSelector.SelectedKeyName = TEXT("DistanceToTarget");
}

void UBTService_FindNearestHostile::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	APawn* Hostile = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
	if (!Hostile || !Hostile->HasAuthority()) return;

	AActor* ClosestActor = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();
	int32 CandidateCount = 0;
	for (TActorIterator<AAuraCharacterBase> It(Hostile->GetWorld()); It; ++It)
	{
		AAuraCharacterBase* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == Hostile || !Candidate->IsCombatAlive()) continue;
		FAuraCombatRuleContext RuleContext;
		RuleContext.QueryPurpose = EAuraCombatQueryPurpose::CombatTargeting;
		RuleContext.TrustedWorldContext = Hostile;
		RuleContext.SourceActor = Hostile;
		RuleContext.TargetActor = Candidate;
		if (const AAuraGameModeBase* GameMode = Hostile->GetWorld()->GetAuthGameMode<AAuraGameModeBase>())
		{
			if (const AAuraBattleDirector* BattleDirector = GameMode->GetBattleDirector())
			{
				FAuraCombatRuleContext AuthoritativeContext;
				if (BattleDirector->ResolveCombatRuleContext(Hostile, Candidate, AuthoritativeContext))
				{
					RuleContext = AuthoritativeContext;
					RuleContext.QueryPurpose = EAuraCombatQueryPurpose::CombatTargeting;
				}
			}
		}
		const FAuraCombatRuleResult Result = FAuraCombatRules::CanCombatTarget(Hostile, Candidate, RuleContext);
		if (!Result.bCanCombatTarget || !Candidate->GetCombatIdentity().bTargetable) continue;
		++CandidateCount;
		const float Distance = Hostile->GetDistanceTo(Candidate);
		if (Distance < ClosestDistance || (FMath::IsNearlyEqual(Distance, ClosestDistance) && GetNameSafe(Candidate) < GetNameSafe(ClosestActor)))
		{
			ClosestDistance = Distance;
			ClosestActor = Candidate;
		}
	}

	UBTFunctionLibrary::SetBlackboardValueAsObject(this, TargetToFollowSelector, ClosestActor);
	UBTFunctionLibrary::SetBlackboardValueAsFloat(this, DistanceToTargetSelector, ClosestDistance);
	if (UWorld* World = Hostile->GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastDebugLogTime >= 10.f)
		{
			LastDebugLogTime = Now;
			UE_LOG(LogAura, Log, TEXT("[EnemyAI][FindNearestHostile] Pawn=%s Candidates=%d Closest=%s Distance=%.1f."),
				*GetNameSafe(Hostile), CandidateCount, *GetNameSafe(ClosestActor), ClosestDistance);
		}
	}
}
