// Copyright Druid Mechanics


#include "AI/AuraAIController.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Pawn.h"

AAuraAIController::AAuraAIController()
{
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>("BlackboardComponent");
	check(Blackboard);
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>("BehaviorTreeComponent");
	check(BehaviorTreeComponent);
}

void AAuraAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Controller] OnPossess: Controller=%s Pawn=%s HasAuthority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InPawn),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AAuraAIController::OnUnPossess()
{
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Controller] OnUnPossess: Controller=%s Pawn=%s HasAuthority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		HasAuthority() ? TEXT("true") : TEXT("false"));
	Super::OnUnPossess();
}
