// Copyright Druid Mechanics

#include "AI/AuraBehaviorUAgentComponent.h"
#include "Character/AuraEnemy.h"
#include "AI/AuraAIController.h"
#include "Aura/AuraLogChannels.h"  // LogAura
#include "BehaviorUTypes.h"          // EBehaviorUStatus
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

UAuraBehaviorUAgentComponent::UAuraBehaviorUAgentComponent()
{
	// Default to the test tree shipped in Content/BehaviorTrees/.
	// /Game/ resolves to <Project>/Content/ inside the plugin's path resolver.
	AutoLoadXMLFilePath = TEXT("/Game/BehaviorTrees/BT_TestEnemy.xml");
	bAutoTick = true;
	WanderRadius = 600.f;
}

void UAuraBehaviorUAgentComponent::BeginPlay()
{
	// Super::BeginPlay registers with the BehaviorU world subsystem and, because
	// AutoLoadXMLFilePath is set, auto-loads the test tree. The tree is not ticked
	// until the subsystem's next frame, so registering handlers / seeding the
	// blackboard afterwards is in time for the first evaluation.
	Super::BeginPlay();

	// Seed the blackboard keys that BT_TestEnemy.xml reads and writes.
	SetIntProperty(TEXT("Energy"), 10);
	SetBoolProperty(TEXT("bCanWander"), true);
	SetVectorProperty(TEXT("WanderTarget"), FVector::ZeroVector);
	SetIntProperty(TEXT("WanderCount"), 0);
	SetPropertyValue(TEXT("LastAction"), TEXT(""));

	// Additional blackboard properties exercised by the tree's blackboard-test
	// branch (bool / string / vector / float / int comparisons).
	SetIntProperty(TEXT("PatrolIndex"), 0);
	SetFloatProperty(TEXT("Health"), 100.f);
	SetBoolProperty(TEXT("bAggressive"), true);
	SetPropertyValue(TEXT("Greeting"), TEXT("Hello"));
	if (AActor* Owner = GetOwner())
	{
		SetVectorProperty(TEXT("HomeLocation"), Owner->GetActorLocation());
	}
	else
	{
		SetVectorProperty(TEXT("HomeLocation"), FVector::ZeroVector);
	}

	// Bind the C++ behavior methods referenced by the tree's <Action> nodes.
	// RegisterMethodHandler keys match the normalized (bare) method names that
	// ExecuteMethod produces, so "PickWanderTarget", "Self.PickWanderTarget"
	// and "Self.PickWanderTarget()" all resolve to the same handler.
	RegisterMethodHandler(TEXT("PickWanderTarget"),  [this]() { return Method_PickWanderTarget();  });
	RegisterMethodHandler(TEXT("MoveToWanderTarget"), [this]() { return Method_MoveToWanderTarget(); });
	RegisterMethodHandler(TEXT("Rest"),              [this]() { return Method_Rest();              });
	RegisterMethodHandler(TEXT("LogState"),          [this]() { return Method_LogState();          });

	UE_LOG(LogAura, Log, TEXT("[BehaviorUTest] Agent bound to %s, auto-loading %s"),
		*GetNameSafe(GetOwner()), *AutoLoadXMLFilePath);
}

EBehaviorUStatus UAuraBehaviorUAgentComponent::Method_PickWanderTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return EBehaviorUStatus::Failure;
	}

	const FVector Origin = Owner->GetActorLocation();
	FVector Target = Origin;

	// Prefer a navmesh-reachable point so the subsequent MoveTo can actually walk there.
	if (UWorld* World = GetWorld())
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation ResultLoc;
			if (NavSys->GetRandomReachablePointInRadius(Origin, WanderRadius, ResultLoc))
			{
				Target = ResultLoc.Location;
			}
		}
	}

	// Fallback: random point on a circle if there is no navmesh (test levels without nav).
	if (Target.Equals(Origin, 1.f))
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		Target = Origin + FVector(FMath::Cos(Angle) * WanderRadius, FMath::Sin(Angle) * WanderRadius, 0.f);
	}

	SetVectorProperty(TEXT("WanderTarget"), Target);

	const int32 Count = GetIntProperty(TEXT("WanderCount")) + 1;
	SetIntProperty(TEXT("WanderCount"), Count);

	UE_LOG(LogAura, Log, TEXT("[BehaviorUTest] %s PickWanderTarget #%d -> %s (Energy=%s, LastAction=%s)"),
		*Owner->GetName(), Count, *Target.ToCompactString(),
		*GetPropertyValue(TEXT("Energy")), *GetPropertyValue(TEXT("LastAction")));

	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAuraBehaviorUAgentComponent::Method_MoveToWanderTarget()
{
	AAuraEnemy* Enemy = Cast<AAuraEnemy>(GetOwner());
	if (!Enemy)
	{
		return EBehaviorUStatus::Failure;
	}

	// Don't compete with the combat AI: while engaged or reacting to a hit, let the
	// existing UE BehaviorTree (combat) drive the NPC. Returning Failure makes the
	// wander Sequence fail this cycle and the outer Loop tries again next cycle.
	if (IsValid(Enemy->CombatTarget) || Enemy->bHitReacting)
	{
		return EBehaviorUStatus::Failure;
	}

	AAIController* AIC = Cast<AAIController>(Enemy->GetController());
	if (!AIC)
	{
		return EBehaviorUStatus::Failure;
	}

	const FVector Target = GetVectorProperty(TEXT("WanderTarget"));
	if (Target.IsNearlyZero(1.f))
	{
		return EBehaviorUStatus::Failure;
	}

	// Fire-and-forget move. ResultOption on the <Action> is BT_SUCCESS, so the node
	// succeeds immediately; the path-follow happens asynchronously on the controller.
	// Levels without a NavMesh (e.g. L_showcase_level) have no navigation data, so a
	// pathfinding move would silently fail and the NPC would never move. Detect that
	// and fall back to a direct (non-pathfinding) move toward the target.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bHasNavMesh = NavSys != nullptr && NavSys->GetDefaultNavDataInstance() != nullptr;
	AIC->MoveToLocation(Target, 50.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/bHasNavMesh,
		/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);

	UE_LOG(LogAura, Log, TEXT("[BehaviorUTest] %s MoveToWanderTarget -> %s"),
		*Enemy->GetName(), *Target.ToCompactString());

	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAuraBehaviorUAgentComponent::Method_Rest()
{
	UE_LOG(LogAura, Log, TEXT("[BehaviorUTest] %s Resting (stamina will be restored by the tree)"),
		*GetNameSafe(GetOwner()));
	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAuraBehaviorUAgentComponent::Method_LogState()
{
	// Throttle: only log every 5th patrol cycle to keep the output readable.
	const int32 PatrolIndex = GetIntProperty(TEXT("PatrolIndex"));
	if (PatrolIndex % 5 != 0)
	{
		return EBehaviorUStatus::Success;
	}

	const FVector WanderTarget = GetVectorProperty(TEXT("WanderTarget"));
	const FVector HomeLocation = GetVectorProperty(TEXT("HomeLocation"));

	UE_LOG(LogAura, Log,
		TEXT("[BehaviorUTest] %s Blackboard: Energy=%s Health=%s PatrolIndex=%d WanderCount=%d "
			 "bCanWander=%s bAggressive=%s Greeting=\"%s\" LastAction=\"%s\" "
			 "WanderTarget=%s HomeLocation=%s"),
		*GetNameSafe(GetOwner()),
		*GetPropertyValue(TEXT("Energy")),
		*GetPropertyValue(TEXT("Health")),
		PatrolIndex,
		GetIntProperty(TEXT("WanderCount")),
		*GetPropertyValue(TEXT("bCanWander")),
		*GetPropertyValue(TEXT("bAggressive")),
		*GetPropertyValue(TEXT("Greeting")),
		*GetPropertyValue(TEXT("LastAction")),
		*WanderTarget.ToCompactString(),
		*HomeLocation.ToCompactString());

	return EBehaviorUStatus::Success;
}