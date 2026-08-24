// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Combat/AuraCombatRuleContext.h"
#include "AuraCivilianAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * Server-only controller for population Civilians.
 *
 * The behavior tree is assembled from native nodes because the current checkout
 * does not contain a valid Civilian BT/BB asset pair. It is still executed by
 * Unreal's normal UBehaviorTreeComponent and BlackboardComponent, not BehaviorU.
 */
UCLASS()
class AURA_API AAuraCivilianAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAuraCivilianAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Called by AAuraCivilian after role and combat state initialization. */
	void StartCivilianBehavior();
	void StopCivilianBehavior();

	/** Builds the directional, server-owned rule query used by civilian threat sensing. */
	FAuraCombatRuleContext BuildThreatRuleContext(const AActor* Candidate, const AActor* Civilian) const;

	UBehaviorTree* GetCivilianBehaviorTree() const { return CivilianBehaviorTree; }
	UBlackboardData* GetCivilianBlackboard() const { return CivilianBlackboard; }
	bool IsCivilianBehaviorStarted() const { return bCivilianBehaviorStarted; }

protected:
	void HandleLifeStateChanged(EAuraCombatLifeState NewState);
	void BuildCivilianBehaviorTree();

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> CivilianBehaviorTree;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardData> CivilianBlackboard;

	bool bCivilianBehaviorStarted = false;
};
