// Copyright Druid Mechanics

#include "AI/BTService_FindNearestThreat.h"

#include "AI/AuraCivilianAIController.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraCharacterBase.h"
#include "Character/AuraCivilian.h"
#include "Combat/AuraCombatRules.h"
#include "Combat/AuraCombatStateComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Game/AuraGameModeBase.h"
#include "World/AuraPopulationManager.h"

UBTService_FindNearestThreat::UBTService_FindNearestThreat()
{
	NodeName = TEXT("Find Nearest Permitted Threat");
	Interval = ServiceInterval;
	RandomDeviation = 0.1f;
}

void UBTService_FindNearestThreat::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AAuraCivilianAIController* Controller = Cast<AAuraCivilianAIController>(OwnerComp.GetAIOwner());
	AAuraCivilian* Civilian = Controller ? Cast<AAuraCivilian>(Controller->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Civilian || !Blackboard || !Civilian->HasAuthority() || !Civilian->IsCombatAlive()) return;

	Blackboard->ClearValue(TEXT("ThreatActor"));
	Blackboard->SetValueAsFloat(TEXT("ThreatDistance"), TNumericLimits<float>::Max());

	const UAuraPopulationManager* Manager = nullptr;
	if (const AAuraGameModeBase* GameMode = Civilian->GetWorld()->GetAuthGameMode<AAuraGameModeBase>())
	{
		Manager = GameMode->GetPopulationManager();
	}
	const FAuraCivilianWorkProfile* Profile = Manager ? Manager->FindWorkProfile(Civilian->GetPopulationMemberState().WorkProfileId) : nullptr;
	const float ThreatRadius = Profile ? Profile->ObserveRadius : 900.f;

	AActor* BestThreat = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AAuraCharacterBase> It(Civilian->GetWorld()); It; ++It)
	{
		AAuraCharacterBase* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == Civilian || !Candidate->IsCombatAlive()) continue;
		const float Distance = FVector::Dist(Civilian->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance > ThreatRadius || Distance > BestDistance) continue;

		FAuraCombatRuleContext RuleContext = Controller->BuildThreatRuleContext(Candidate, Civilian);
		const FAuraCombatRuleResult Result = FAuraCombatRules::CanDamage(Candidate, Civilian, RuleContext);
		if (!Result.bCanDamage) continue;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AuraCivilianThreat), true);
		QueryParams.AddIgnoredActor(Civilian);
		FHitResult Hit;
		const bool bBlocked = Civilian->GetWorld()->LineTraceSingleByChannel(Hit, Candidate->GetActorLocation(), Civilian->GetActorLocation(), ECC_Visibility, QueryParams)
			&& Hit.GetActor() != Candidate;
		if (bBlocked) continue;

		BestThreat = Candidate;
		BestDistance = Distance;
	}

	if (BestThreat)
	{
		Blackboard->SetValueAsObject(TEXT("ThreatActor"), BestThreat);
		Blackboard->SetValueAsFloat(TEXT("ThreatDistance"), BestDistance);
		UE_LOG(LogAura, Verbose, TEXT("[CivilianAI][Threat] civilian=%s threat=%s distance=%.1f."),
			*GetNameSafe(Civilian), *GetNameSafe(BestThreat), BestDistance);
	}
}
