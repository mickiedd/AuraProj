// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraAttackTimelineTypes.h"
#include "Gameplay/AuraEvadePolicy.h"

namespace AuraGameplayDay44TestsPrivate
{
	FAuraAttackTimelineKey MakeAttackKey(int32 EncounterGeneration = 1, int32 SourceLifeGeneration = 1,
		int32 AttackSequence = 1)
	{
		FAuraAttackTimelineKey Key;
		Key.RunId = FGuid(1, 2, 3, 4);
		Key.Epoch = 1;
		Key.EncounterGeneration = EncounterGeneration;
		Key.EncounterId = TEXT("cell_1");
		Key.DriverOwner = TEXT("MissionEncounterDriver");
		Key.SourceEntityId = TEXT("enemy_1");
		Key.SourceLifeGeneration = SourceLifeGeneration;
		Key.AttackSequence = AttackSequence;
		return Key;
	}

	FAuraAttackTimelineDefinition MakeAttackDefinition()
	{
		FAuraAttackTimelineDefinition Definition;
		Definition.AttackDefinitionId = TEXT("RaiderHeavyStrike");
		Definition.AttackShapeId = TEXT("shape_raider_heavy_arc");
		Definition.TelegraphProfileId = TEXT("telegraph_heavy_arc");
		Definition.WindupSeconds = 0.85f;
		Definition.DamageWindowSeconds = 0.05f;
		Definition.RecoverySeconds = 1.0f;
		Definition.bShapeFrozenDuringWindup = true;
		Definition.bUsesServerFallbackTiming = true;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44CuePrecedesImpact,
	"Aura.Gameplay.Day44.CuePrecedesImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44CuePrecedesImpact::RunTest(const FString& Parameters)
{
	FAuraAttackTimelineState State;
	const FAuraAttackTimelineKey Key = AuraGameplayDay44TestsPrivate::MakeAttackKey();
	const FAuraAttackTimelineDefinition Definition = AuraGameplayDay44TestsPrivate::MakeAttackDefinition();
	FString Error;
	TestEqual(TEXT("Attack enters authoritative windup"),
		State.TryBeginWindup(Key, Definition, 10.0, Error), EAuraAttackTimelineResult::Started);
	TestTrue(TEXT("Windup is at least the 0.85 second contract"),
		State.ExpectedImpactServerTime - State.AuthorityStartServerTime >= 0.85 - KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Commit before impact is rejected"), State.TryCommit(Key, 10.84, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Commit at server impact time is accepted"), State.TryCommit(Key, 10.85, Error),
		EAuraAttackTimelineResult::Committed);
	TestEqual(TEXT("Impact resolves once inside its window"), State.TryResolveImpact(Key, 10.85, Error),
		EAuraAttackTimelineResult::ImpactResolved);
	TestEqual(TEXT("Duplicate impact resolution is rejected"), State.TryResolveImpact(Key, 10.86, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Recovery finishes only after its authoritative deadline"), State.TryFinishRecovery(Key, 11.91, Error),
		EAuraAttackTimelineResult::RecoveryFinished);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44AttackCallbackDeadline,
	"Aura.Gameplay.Day44.AttackCallbackDeadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44AttackCallbackDeadline::RunTest(const FString& Parameters)
{
	FAuraAttackTimelineState State;
	const FAuraAttackTimelineKey Key = AuraGameplayDay44TestsPrivate::MakeAttackKey();
	const FAuraAttackTimelineDefinition Definition = AuraGameplayDay44TestsPrivate::MakeAttackDefinition();
	FString Error;
	TestEqual(TEXT("Attack begins before the timeout guard is armed"), State.TryBeginWindup(Key, Definition, 40.0, Error),
		EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("A callback inside the deadline can commit"), State.TryCommit(Key, 40.85, Error),
		EAuraAttackTimelineResult::Committed);
	TestEqual(TEXT("A callback past the damage window cancels the stranded timeline"), State.TryResolveImpact(Key, 40.91, Error),
		EAuraAttackTimelineResult::Cancelled);
	TestEqual(TEXT("The late impact cancellation is terminal"), State.Phase, EAuraAttackTimelinePhase::Cancelled);
	TestEqual(TEXT("The cancellation reason is observable for lease cleanup"), State.TerminalReason,
		FName(TEXT("AttackImpactWindowExpired")));

	FAuraAttackTimelineState TimeoutState;
	TestEqual(TEXT("A second attack begins for the callback deadline case"),
		TimeoutState.TryBeginWindup(Key, Definition, 50.0, Error), EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("A callback beyond recovery plus one second settles as timeout"),
		TimeoutState.TryCommit(Key, 53.0, Error), EAuraAttackTimelineResult::Cancelled);
	TestEqual(TEXT("Timeout settlement records a typed reason"), TimeoutState.TerminalReason,
		FName(TEXT("AttackCallbackTimeout")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44DeadAttackerCancelsImpact,
	"Aura.Gameplay.Day44.DeadAttackerCancelsImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44DeadAttackerCancelsImpact::RunTest(const FString& Parameters)
{
	FAuraAttackTimelineState State;
	const FAuraAttackTimelineKey Key = AuraGameplayDay44TestsPrivate::MakeAttackKey();
	const FAuraAttackTimelineDefinition Definition = AuraGameplayDay44TestsPrivate::MakeAttackDefinition();
	FString Error;
	State.TryBeginWindup(Key, Definition, 10.0, Error);
	const FAuraAttackTimelineKey StaleLifeKey = AuraGameplayDay44TestsPrivate::MakeAttackKey(1, 2, 1);
	TestEqual(TEXT("Old source life cannot commit the replacement attack"), State.TryCommit(StaleLifeKey, 10.85, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Stale callback leaves the real attack in windup"), State.Phase, EAuraAttackTimelinePhase::Windup);
	TestEqual(TEXT("Death cancellation prevents a later impact"), State.TryCancel(Key, TEXT("SourceDeath"), Error),
		EAuraAttackTimelineResult::Cancelled);
	TestEqual(TEXT("Cancelled attack cannot resolve damage"), State.TryResolveImpact(Key, 10.85, Error),
		EAuraAttackTimelineResult::Rejected);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44CancelledTimelineRequiresExplicitReset,
	"Aura.Gameplay.Day44.CancelledTimelineRequiresExplicitReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44CancelledTimelineRequiresExplicitReset::RunTest(const FString& Parameters)
{
	FAuraAttackTimelineState State;
	const FAuraAttackTimelineKey CancelledKey = AuraGameplayDay44TestsPrivate::MakeAttackKey(1, 1, 1);
	const FAuraAttackTimelineKey ReplacementKey = AuraGameplayDay44TestsPrivate::MakeAttackKey(1, 1, 2);
	const FAuraAttackTimelineDefinition Definition = AuraGameplayDay44TestsPrivate::MakeAttackDefinition();
	FString Error;
	TestEqual(TEXT("The original attack begins before cancellation"), State.TryBeginWindup(CancelledKey, Definition, 20.0, Error),
		EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("Cancellation remains observable as a terminal state"), State.TryCancel(CancelledKey, TEXT("SourceDeath"), Error),
		EAuraAttackTimelineResult::Cancelled);
	TestEqual(TEXT("A cancelled timeline cannot be reused without an explicit reset"),
		State.TryBeginWindup(ReplacementKey, Definition, 20.0, Error), EAuraAttackTimelineResult::Rejected);

	State.Reset();
	TestEqual(TEXT("Explicit reset allows the next attack to begin"), State.TryBeginWindup(ReplacementKey, Definition, 20.0, Error),
		EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("A late callback from the cancelled attack cannot affect the replacement"),
		State.TryResolveImpact(CancelledKey, 20.85, Error), EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("The replacement remains in authoritative windup after the stale callback"), State.Phase,
		EAuraAttackTimelinePhase::Windup);
	TestEqual(TEXT("The replacement can commit on its own server deadline"), State.TryCommit(ReplacementKey, 20.85, Error),
		EAuraAttackTimelineResult::Committed);

	FAuraAttackTimelineState ActiveState;
	TestEqual(TEXT("A separate active timeline begins for reset-boundary coverage"),
		ActiveState.TryBeginWindup(CancelledKey, Definition, 30.0, Error), EAuraAttackTimelineResult::Started);
	ActiveState.Reset();
	TestEqual(TEXT("Reset cannot silently clear an active windup"), ActiveState.Phase, EAuraAttackTimelinePhase::Windup);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44EvadeRequestAbuse,
	"Aura.Gameplay.Day44.EvadeRequestAbuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44EvadeRequestAbuse::RunTest(const FString& Parameters)
{
	FAuraEvadeState InitialState;
	TestTrue(TEXT("Evade state accepts a valid owner identity"),
		InitialState.Initialize(FGuid(1, 2, 3, 4), 1, TEXT("player_1"), 1));
	FAuraEvadeRequest Request;
	Request.Key.RunId = InitialState.RunId;
	Request.Key.Epoch = InitialState.Epoch;
	Request.Key.OwnerId = InitialState.OwnerId;
	Request.Key.OwnerLifeGeneration = InitialState.OwnerLifeGeneration;
	Request.Key.RequestSequence = 1;
	Request.Direction = FVector(1.0f, 0.0f, 0.0f);
	FAuraEvadeOwnerSnapshot OwnerSnapshot;
	OwnerSnapshot.bAlive = true;
	OwnerSnapshot.bRunEligible = true;
	FAuraEvadeState AcceptedState;
	FAuraEvadeAcceptance Acceptance;
	FString Error;
	TestEqual(TEXT("Valid evade is admitted by the policy"),
		FAuraEvadePolicy::TryAccept(InitialState, Request, OwnerSnapshot, true, 10.0, AcceptedState, Acceptance, Error),
		EAuraEvadeResult::Accepted);
	TestEqual(TEXT("Accepted evade consumes its only charge"), AcceptedState.ChargesRemaining, 0);
	TestEqual(TEXT("Fully blocked sweep remains an accepted result"),
		FAuraEvadePolicy::ResolveSweep(AcceptedState, Request.Key, 10.20, 0.0f, true, AcceptedState, Error),
		EAuraEvadeSweepResult::AcceptedBlocked);

	FAuraEvadeState DuplicateState;
	FAuraEvadeAcceptance DuplicateAcceptance;
	TestEqual(TEXT("Duplicate delivery is rejected without another cancellation"),
		FAuraEvadePolicy::TryAccept(AcceptedState, Request, OwnerSnapshot, true, 10.21, DuplicateState,
			DuplicateAcceptance, Error), EAuraEvadeResult::RejectedDuplicate);
	TestFalse(TEXT("Duplicate delivery has no interaction cancellation"),
		DuplicateAcceptance.CancellationPlan.bCancelOwnedInteraction);

	FAuraEvadeRequest RapidRetry = Request;
	RapidRetry.Key.RequestSequence = 2;
	FAuraEvadeAcceptance RetryAcceptance;
	TestEqual(TEXT("Rapid retry is held by the server cooldown"),
		FAuraEvadePolicy::TryAccept(AcceptedState, RapidRetry, OwnerSnapshot, true, 10.21, DuplicateState,
			RetryAcceptance, Error), EAuraEvadeResult::RejectedCooldown);
	FAuraEvadeRequest InvalidDirection = RapidRetry;
	InvalidDirection.Key.RequestSequence = 3;
	InvalidDirection.Direction = FVector(350.0f, 0.0f, 0.0f);
	FAuraEvadeState InvalidDirectionState;
	TestEqual(TEXT("Oversized direction is rejected after cooldown expires"),
		FAuraEvadePolicy::TryAccept(AcceptedState, InvalidDirection, OwnerSnapshot, true, 13.0,
			InvalidDirectionState, RetryAcceptance, Error), EAuraEvadeResult::RejectedInvalidDirection);
	TestEqual(TEXT("Invalid direction does not create another accepted request"), InvalidDirectionState.AcceptedRequestCount, 1);
	TestEqual(TEXT("Invalid direction does not restore or consume a charge"), InvalidDirectionState.ChargesRemaining, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44EvadeMovementWindow,
	"Aura.Gameplay.Day44.EvadeMovementWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44EvadeMovementWindow::RunTest(const FString& Parameters)
{
	FAuraEvadeState State;
	TestTrue(TEXT("Evade state initializes"), State.Initialize(FGuid(1, 2, 3, 4), 1, TEXT("player_1"), 1));
	FAuraEvadeRequest Request;
	Request.Key.RunId = State.RunId;
	Request.Key.Epoch = State.Epoch;
	Request.Key.OwnerId = State.OwnerId;
	Request.Key.OwnerLifeGeneration = State.OwnerLifeGeneration;
	Request.Key.RequestSequence = 1;
	Request.Direction = FVector::ForwardVector;
	FAuraEvadeOwnerSnapshot OwnerSnapshot;
	OwnerSnapshot.bAlive = true;
	OwnerSnapshot.bRunEligible = true;
	FAuraEvadeState AcceptedState;
	FAuraEvadeAcceptance Acceptance;
	FString Error;
	TestEqual(TEXT("Movement request is accepted"), FAuraEvadePolicy::TryAccept(State, Request, OwnerSnapshot, true,
		10.0, AcceptedState, Acceptance, Error), EAuraEvadeResult::Accepted);
	FAuraEvadeState BeforeLateSweep = AcceptedState;
	TestEqual(TEXT("A sweep after the 0.25-second movement window is rejected"), FAuraEvadePolicy::ResolveSweep(
		AcceptedState, Request.Key, 10.251, 100.0f, false, AcceptedState, Error),
		EAuraEvadeSweepResult::RejectedOutsideMovementWindow);
	TestFalse(TEXT("Late sweep does not settle movement"), AcceptedState.bMovementResolved);
	TestEqual(TEXT("A sweep inside the authoritative window remains accepted"), FAuraEvadePolicy::ResolveSweep(
		BeforeLateSweep, Request.Key, 10.25, 100.0f, false, AcceptedState, Error),
		EAuraEvadeSweepResult::AcceptedFullDistance);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay44EvadeCancellationPlan,
	"Aura.Gameplay.Day44.EvadeCancellationPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay44EvadeCancellationPlan::RunTest(const FString& Parameters)
{
	FAuraEvadeState State;
	State.Initialize(FGuid(1, 2, 3, 4), 1, TEXT("player_1"), 1);
	FAuraEvadeRequest Request;
	Request.Key.RunId = State.RunId;
	Request.Key.Epoch = State.Epoch;
	Request.Key.OwnerId = State.OwnerId;
	Request.Key.OwnerLifeGeneration = State.OwnerLifeGeneration;
	Request.Key.RequestSequence = 1;
	Request.Direction = FVector::ForwardVector;
	FAuraEvadeOwnerSnapshot OwnerSnapshot;
	OwnerSnapshot.bAlive = true;
	OwnerSnapshot.bRunEligible = true;
	FAuraEvadeState AcceptedState;
	FAuraEvadeAcceptance Acceptance;
	FString Error;
	TestEqual(TEXT("Acceptance returns one owner cancellation plan"),
		FAuraEvadePolicy::TryAccept(State, Request, OwnerSnapshot, true, 10.0, AcceptedState, Acceptance, Error),
		EAuraEvadeResult::Accepted);
	TestTrue(TEXT("Plan cancels owned interaction"), Acceptance.CancellationPlan.bCancelOwnedInteraction);
	TestTrue(TEXT("Plan cancels reload through the future PlayerState adapter"), Acceptance.CancellationPlan.bCancelReload);
	TestTrue(TEXT("Plan cancels an owned channel"), Acceptance.CancellationPlan.bCancelOwnedChannel);
	TestFalse(TEXT("Plan never grants invulnerability"), Acceptance.CancellationPlan.bGrantsInvulnerability);
	TestTrue(TEXT("Acceptance commits a cooldown before movement"), Acceptance.CooldownReadyServerTime > Acceptance.AcceptedAtServerTime);
	return !HasAnyErrors();
}

#endif
