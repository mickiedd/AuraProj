// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraEscortTypes.h"

namespace AuraGameplayDay49TestsPrivate
{
	FGuid RunId(int32 Suffix = 4)
	{
		return FGuid(49, 2026, 9, Suffix);
	}

	TArray<FName> RequiredMembers()
	{
		return { TEXT("civilian_rescue_a"), TEXT("civilian_rescue_b") };
	}

	TArray<FAuraEscortMemberSnapshot> AvailableMembers()
	{
		TArray<FAuraEscortMemberSnapshot> Members;
		FAuraEscortMemberSnapshot& First = Members.AddDefaulted_GetRef();
		First.MemberId = TEXT("civilian_rescue_a");
		First.LifeGeneration = 7;
		First.bAlive = true;
		FAuraEscortMemberSnapshot& Second = Members.AddDefaulted_GetRef();
		Second.MemberId = TEXT("civilian_rescue_b");
		Second.LifeGeneration = 8;
		Second.bAlive = true;
		return Members;
	}

	bool Initialize(FAuraEscortRunState& State, int32 Suffix = 4, double StartTime = 0.0,
		double DeadlineSeconds = 120.0, bool bAuthority = true)
	{
		FString Error;
		return State.Initialize(bAuthority, RunId(Suffix), 1, 1, TEXT("RescueRelay"), TEXT("arrangement_a"),
			StartTime, DeadlineSeconds, RequiredMembers(), Error);
	}

	EAuraEscortResult Prepare(FAuraEscortRunState& State, FAuraEscortReservationLedger& Ledger, FString& Error)
	{
		return State.TryPrepare(true, Ledger, TEXT("shelter_a_1"), AvailableMembers(), Error);
	}

	FAuraEscortLayoutDefinition MakeLayout(FName Id, FName FirstShelter, FName SecondShelter)
	{
		FAuraEscortLayoutDefinition Layout;
		Layout.ArrangementId = Id;
		Layout.ShelterWaypointIds = { FirstShelter, SecondShelter };
		Layout.bAuraRouteReachable = true;
		Layout.bBungeeManRouteReachable = true;
		Layout.bCoverRegistered = true;
		Layout.bNavPathVerified = true;
		Layout.bEscortLOSVerified = true;
		Layout.bHubExcluded = false;
		Layout.bSpawnConstraintsVerified = true;
		return Layout;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49ReservationPreventsTwoOwners,
	"Aura.Gameplay.Day49.ReservationPreventsTwoOwners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49ReservationPreventsTwoOwners::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FAuraEscortRunState FirstRun;
	FAuraEscortRunState SecondRun;
	FString Error;
	TestTrue(TEXT("Authority run initializes"), AuraGameplayDay49TestsPrivate::Initialize(FirstRun));
	TestEqual(TEXT("First owner reserves both designated civilians"),
		AuraGameplayDay49TestsPrivate::Prepare(FirstRun, Ledger, Error), EAuraEscortResult::Reserved);
	TestTrue(TEXT("First reservation is active"), Ledger.IsReserved(TEXT("civilian_rescue_a"))
		&& Ledger.IsReserved(TEXT("civilian_rescue_b")));
	TestTrue(TEXT("Second authority run initializes"), AuraGameplayDay49TestsPrivate::Initialize(SecondRun, 5));
	TestEqual(TEXT("A second run cannot steal the active reservation"),
		AuraGameplayDay49TestsPrivate::Prepare(SecondRun, Ledger, Error), EAuraEscortResult::Rejected);
	TestEqual(TEXT("The original owner can replay preparation idempotently"),
		AuraGameplayDay49TestsPrivate::Prepare(FirstRun, Ledger, Error), EAuraEscortResult::AlreadyReserved);
	TestEqual(TEXT("A client cannot replay an authority reservation"), FirstRun.TryPrepare(
		false, Ledger, TEXT("shelter_a_1"), AuraGameplayDay49TestsPrivate::AvailableMembers(), Error),
		EAuraEscortResult::Rejected);
	TestTrue(TEXT("Rejected client replay preserves both authority reservations"),
		Ledger.IsReserved(TEXT("civilian_rescue_a")) && Ledger.IsReserved(TEXT("civilian_rescue_b")));

	FAuraEscortReservationLedger AtomicLedger;
	FAuraEscortRunState AtomicRun;
	TestTrue(TEXT("Atomicity run initializes"), AuraGameplayDay49TestsPrivate::Initialize(AtomicRun, 7));
	TArray<FAuraEscortMemberSnapshot> IncompleteMembers = AuraGameplayDay49TestsPrivate::AvailableMembers();
	IncompleteMembers[1].bAlive = false;
	TestEqual(TEXT("Preparation rejects when either designated civilian is unavailable"), AtomicRun.TryPrepare(
		true, AtomicLedger, TEXT("shelter_a_1"), IncompleteMembers, Error), EAuraEscortResult::Rejected);
	TestFalse(TEXT("A failed two-member reservation leaves no partial first lease"),
		AtomicLedger.IsReserved(TEXT("civilian_rescue_a")));
	TestFalse(TEXT("A failed two-member reservation leaves no partial second lease"),
		AtomicLedger.IsReserved(TEXT("civilian_rescue_b")));
	FAuraEscortRunState ClientRun;
	TestFalse(TEXT("A client cannot initialize an authority escort reducer"),
		AuraGameplayDay49TestsPrivate::Initialize(ClientRun, 6, 0.0, 120.0, false));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49EscortPolicyScoped,
	"Aura.Gameplay.Day49.EscortPolicyScoped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49EscortPolicyScoped::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FAuraEscortRunState State;
	FString Error;
	TestTrue(TEXT("Policy run initializes"), AuraGameplayDay49TestsPrivate::Initialize(State));
	TestEqual(TEXT("Policy run reserves designated members"),
		AuraGameplayDay49TestsPrivate::Prepare(State, Ledger, Error), EAuraEscortResult::Reserved);
	TestTrue(TEXT("A mission enemy may target a designated live escort"), State.CanMissionEnemyDamage(
		true, true, false, TEXT("civilian_rescue_a"), 7, Error));
	TestFalse(TEXT("A player may not damage a designated civilian"), State.CanMissionEnemyDamage(
		true, false, true, TEXT("civilian_rescue_a"), 7, Error));
	TestFalse(TEXT("A non-mission enemy may not target the escort contract"), State.CanMissionEnemyDamage(
		true, false, false, TEXT("civilian_rescue_a"), 7, Error));
	TestFalse(TEXT("An unrelated civilian is outside the mission damage scope"), State.CanMissionEnemyDamage(
		true, true, false, TEXT("civilian_other"), 7, Error));
	TestFalse(TEXT("A replaced escort life cannot be damaged through the old lease"), State.CanMissionEnemyDamage(
		true, true, false, TEXT("civilian_rescue_a"), 6, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49EscortArrivalExactlyOnce,
	"Aura.Gameplay.Day49.EscortArrivalExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49EscortArrivalExactlyOnce::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FAuraEscortRunState State;
	FString Error;
	TestTrue(TEXT("Arrival run initializes"), AuraGameplayDay49TestsPrivate::Initialize(State));
	TestEqual(TEXT("Arrival run reserves both members"),
		AuraGameplayDay49TestsPrivate::Prepare(State, Ledger, Error), EAuraEscortResult::Reserved);
	TestEqual(TEXT("A wrong destination volume cannot count arrival"), State.RegisterArrival(
		true, TEXT("civilian_rescue_a"), 7, TEXT("shelter_b_1"), 2.0, Error), EAuraEscortResult::Rejected);
	TestEqual(TEXT("A stale life generation cannot count arrival"), State.RegisterArrival(
		true, TEXT("civilian_rescue_a"), 6, TEXT("shelter_a_1"), 2.0, Error), EAuraEscortResult::Rejected);
	TestEqual(TEXT("The first designated arrival counts once"), State.RegisterArrival(
		true, TEXT("civilian_rescue_a"), 7, TEXT("shelter_a_1"), 3.0, Error), EAuraEscortResult::ArrivalAccepted);
	TestEqual(TEXT("The same overlap event is idempotent"), State.RegisterArrival(
		true, TEXT("civilian_rescue_a"), 7, TEXT("shelter_a_1"), 3.1, Error), EAuraEscortResult::DuplicateArrival);
	TestEqual(TEXT("The second designated arrival completes the escort"), State.RegisterArrival(
		true, TEXT("civilian_rescue_b"), 8, TEXT("shelter_a_1"), 4.0, Error), EAuraEscortResult::Completed);
	TestEqual(TEXT("Both required members are counted exactly once"), State.GetArrivedCount(), 2);
	TestEqual(TEXT("Completed escort is terminal"), State.GetPhase(), EAuraEscortPhase::Completed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49StallAbortBounded,
	"Aura.Gameplay.Day49.StallAbortBounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49StallAbortBounded::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FAuraEscortRunState State;
	FString Error;
	TestTrue(TEXT("Stall run initializes"), AuraGameplayDay49TestsPrivate::Initialize(State, 4, 0.0, 60.0));
	TestEqual(TEXT("Stall run reserves its members"),
		AuraGameplayDay49TestsPrivate::Prepare(State, Ledger, Error), EAuraEscortResult::Reserved);
	TestEqual(TEXT("First retry is requested at five seconds of blocked movement"), State.TickMovement(
		true, 5.0, true, true, false, false, Error), EAuraEscortResult::RetryRequested);
	TestEqual(TEXT("A successful path query without actual movement does not reset the stall clock"),
		State.TickMovement(true, 6.0, true, true, false, false, Error), EAuraEscortResult::Moving);
	TestTrue(TEXT("Requested-movement stall time continues accumulating"),
		FMath::IsNearlyEqual(State.GetStallSeconds(), 6.0));
	TestEqual(TEXT("Second retry is requested at seven seconds"), State.TickMovement(
		true, 7.0, true, true, false, false, Error), EAuraEscortResult::RetryRequested);
	TestEqual(TEXT("A blocked path aborts no later than nine seconds"), State.TickMovement(
		true, 9.0, true, true, false, false, Error), EAuraEscortResult::TechnicalAbort);
	TestEqual(TEXT("Stall abort has a technical terminal phase"), State.GetPhase(), EAuraEscortPhase::Aborted);
	TestEqual(TEXT("Stall abort exposes a typed reason"), State.GetTerminalReason(), FName(TEXT("EscortPathStallAbort")));
	TestTrue(TEXT("No actual progress was fabricated"), FMath::IsNearlyEqual(State.GetLastActualProgressServerTime(), 0.0));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49IntentionalEscortWaitIsNotStall,
	"Aura.Gameplay.Day49.IntentionalEscortWaitIsNotStall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49IntentionalEscortWaitIsNotStall::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FAuraEscortRunState State;
	FString Error;
	TestTrue(TEXT("Wait run initializes"), AuraGameplayDay49TestsPrivate::Initialize(State, 4, 0.0, 120.0));
	TestEqual(TEXT("Wait run reserves its members"),
		AuraGameplayDay49TestsPrivate::Prepare(State, Ledger, Error), EAuraEscortResult::Reserved);
	TestEqual(TEXT("Initial blocked movement reaches the first retry"), State.TickMovement(
		true, 5.0, true, true, false, false, Error), EAuraEscortResult::RetryRequested);
	TestEqual(TEXT("An intentional shelter wait pauses the stall clock"), State.TickMovement(
		true, 14.0, true, false, true, false, Error), EAuraEscortResult::Sheltered);
	TestEqual(TEXT("Shelter wait does not add wall-clock stall time"), State.GetStallSeconds(), 5.0);
	TestEqual(TEXT("Leaving the follow range enters a waiting state"), State.TickMovement(
		true, 20.0, false, false, false, false, Error), EAuraEscortResult::WaitingForEscort);
	TestEqual(TEXT("Actual progress after return resumes movement and resets retries"), State.TickMovement(
		true, 21.0, true, true, false, true, Error), EAuraEscortResult::Moving);
	TestEqual(TEXT("Blocked movement after return starts a fresh stall window"), State.TickMovement(
		true, 26.0, true, true, false, false, Error), EAuraEscortResult::RetryRequested);
	TestEqual(TEXT("The run deadline continues during an intentional wait"), State.TickMovement(
		true, 121.0, true, false, true, false, Error), EAuraEscortResult::Failed);
	TestEqual(TEXT("Deadline failure is distinct from path stall"), State.GetTerminalReason(), FName(TEXT("EscortDeadlineExpired")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49ReleaseRestoresCivilianWork,
	"Aura.Gameplay.Day49.ReleaseRestoresCivilianWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49ReleaseRestoresCivilianWork::RunTest(const FString& Parameters)
{
	FAuraEscortReservationLedger Ledger;
	FString Error;
	FAuraEscortRunState Success;
	TestTrue(TEXT("Successful run initializes"), AuraGameplayDay49TestsPrivate::Initialize(Success, 4));
	TestEqual(TEXT("Successful run reserves civilians"),
		AuraGameplayDay49TestsPrivate::Prepare(Success, Ledger, Error), EAuraEscortResult::Reserved);
	Success.RegisterArrival(true, TEXT("civilian_rescue_a"), 7, TEXT("shelter_a_1"), 1.0, Error);
	TestEqual(TEXT("Successful run completes"), Success.RegisterArrival(
		true, TEXT("civilian_rescue_b"), 8, TEXT("shelter_a_1"), 1.0, Error), EAuraEscortResult::Completed);
	TestEqual(TEXT("Successful completion releases the destination lease"), Success.ReleaseReservations(
		true, Ledger, Error), EAuraEscortResult::Released);
	TestFalse(TEXT("Successful release removes the active member lease"), Ledger.IsReserved(TEXT("civilian_rescue_a")));
	TestTrue(TEXT("Successful release restores normal civilian work"), Ledger.IsWorkRestored(TEXT("civilian_rescue_a")));
	TestEqual(TEXT("Repeated release is idempotent"), Success.ReleaseReservations(
		true, Ledger, Error), EAuraEscortResult::AlreadyReleased);

	FAuraEscortRunState Failed;
	TestTrue(TEXT("Failed run initializes"), AuraGameplayDay49TestsPrivate::Initialize(Failed, 5));
	TestEqual(TEXT("Failed run reserves civilians"),
		AuraGameplayDay49TestsPrivate::Prepare(Failed, Ledger, Error), EAuraEscortResult::Reserved);
	TestEqual(TEXT("Civilian death fails the escort once"), Failed.RegisterCivilianDeath(
		true, TEXT("civilian_rescue_a"), 7, 2.0, Error), EAuraEscortResult::Failed);
	TestEqual(TEXT("Failure releases its reservations"), Failed.ReleaseReservations(
		true, Ledger, Error), EAuraEscortResult::Released);
	TestTrue(TEXT("Failure restores civilian work"), Ledger.IsWorkRestored(TEXT("civilian_rescue_b")));

	FAuraEscortRunState Aborted;
	TestTrue(TEXT("Technical-abort run initializes"), AuraGameplayDay49TestsPrivate::Initialize(Aborted, 6, 0.0, 60.0));
	TestEqual(TEXT("Technical-abort run reserves civilians"),
		AuraGameplayDay49TestsPrivate::Prepare(Aborted, Ledger, Error), EAuraEscortResult::Reserved);
	TestEqual(TEXT("Technical path stall aborts"), Aborted.TickMovement(
		true, 9.0, true, true, false, false, Error), EAuraEscortResult::TechnicalAbort);
	TestEqual(TEXT("Technical abort releases its reservations"), Aborted.ReleaseReservations(
		true, Ledger, Error), EAuraEscortResult::Released);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay49LayoutsReachableByBothRoles,
	"Aura.Gameplay.Day49.LayoutsReachableByBothRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay49LayoutsReachableByBothRoles::RunTest(const FString& Parameters)
{
	TArray<FAuraEscortLayoutDefinition> Layouts;
	Layouts.Add(AuraGameplayDay49TestsPrivate::MakeLayout(TEXT("arrangement_a"), TEXT("shelter_a_1"), TEXT("shelter_a_2")));
	Layouts.Add(AuraGameplayDay49TestsPrivate::MakeLayout(TEXT("arrangement_b"), TEXT("shelter_b_1"), TEXT("shelter_b_2")));
	FString Error;
	TestTrue(TEXT("Both arrangements pass the explicit fixture contract"), FAuraEscortLayoutRules::Validate(Layouts, Error));
	Layouts[1].bBungeeManRouteReachable = false;
	TestFalse(TEXT("A role-unreachable arrangement fails closed"), FAuraEscortLayoutRules::Validate(Layouts, Error));
	return !HasAnyErrors();
}

#endif
