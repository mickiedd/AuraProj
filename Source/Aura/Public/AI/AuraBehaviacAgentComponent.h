// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorUAgent.h"
#include "AuraBehaviacAgentComponent.generated.h"

class AAuraEnemy;

/**
 * UAuraBehaviacAgentComponent
 *
 * Aura-specific subclass of the BehaviorU plugin's UBehaviorUAgentComponent.
 *
 * It wires the test behavior tree (BT_TestEnemy.xml) to an AuraEnemy by:
 *  - defaulting AutoLoadXMLFilePath to the test tree so the NPC auto-loads it on
 *    BeginPlay (no Blueprint setup required);
 *  - initializing the blackboard keys the tree reads/writes (Energy, bCanWander,
 *    WanderTarget, WanderCount, LastAction);
 *  - registering the C++ "behavior methods" that the tree's <Action Method="...">
 *    nodes invoke through the plugin's command queue (PickWanderTarget,
 *    MoveToWanderTarget, Rest).
 *
 * The methods execute on the game thread (Phase 1 of the BehaviorU world subsystem
 * tick), so they may safely touch the owning Actor and its AIController.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent), DisplayName = "Aura BehaviorU Agent")
class AURA_API UAuraBehaviacAgentComponent : public UBehaviorUAgentComponent
{
	GENERATED_BODY()

public:
	UAuraBehaviacAgentComponent();

	virtual void BeginPlay() override;

	/** Radius (cm) around the NPC used when picking a wander target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Wander")
	float WanderRadius;

protected:
	// --- Behavior methods bound to BT_TestEnemy.xml ---
	// Each is registered in BeginPlay via RegisterMethodHandler and returns an
	// EBehaviorUStatus that the calling <Action> node forwards through the tree.

	/** Pick a random reachable point near the NPC and store it in WanderTarget. */
	EBehaviorUStatus Method_PickWanderTarget();

	/** Issue a MoveToLocation request to the NPC's AIController for WanderTarget.
	 *  Skipped (returns Failure) while the NPC is in combat or hit-reacting so the
	 *  BehaviorU tree does not fight the existing combat BehaviorTree. */
	EBehaviorUStatus Method_MoveToWanderTarget();

	/** "Rest" placeholder — logs that the NPC is resting. Stamina is restored by
	 *  the tree's Assignment node, not by this method. */
	EBehaviorUStatus Method_Rest();

	/** Dump the full blackboard to the log so test property values are visible.
	 *  Throttled to every 5th patrol cycle to avoid log spam. */
	EBehaviorUStatus Method_LogState();
};