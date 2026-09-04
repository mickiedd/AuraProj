// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraRunRecoveryTypes.h"

namespace AuraGameplayDay51TestsPrivate
{
	FAuraRunRecoveryRetainedState Retained(int32 Offset = 0)
	{
		FAuraRunRecoveryRetainedState State;
		State.MagazineRounds = 7 + Offset;
		State.ReserveRounds = 31 + Offset;
		State.AmmoRevision = 9 + Offset;
		State.SupplyCount = 2 + Offset;
		State.AugmentIds = { TEXT("augment_alpha"), TEXT("augment_beta") };
		State.AbilityCooldownEndServerTime = 40.0 + Offset;
		State.EvadeCooldownEndServerTime = 20.0 + Offset;
		State.SupplyCooldownEndServerTime = 60.0 + Offset;
		return State;
	}

	FAuraRunRecoveryMember Member(FName Id, FName Role, int32 Offset = 0)
	{
		FAuraRunRecoveryMember Result;
		Result.MemberId = Id;
		Result.RoleId = Role;
		Result.RetainedState = Retained(Offset);
		return Result;
	}

	TArray<FAuraRunRecoveryMember> Pair()
	{
		return { Member(TEXT("player_a"), TEXT("Aura")), Member(TEXT("player_b"), TEXT("BungeeMan"), 1) };
	}

	bool InitializePair(FAuraRunRecoveryState& State, int32 Suffix = 51)
	{
		FString Error;
		return State.Initialize(true, FGuid(51, 2026, 9, Suffix), 1, TEXT("GameplayExpansionV1"),
			2, 0.0, 300.0, Pair(), Error);
	}

	bool InitializeSolo(FAuraRunRecoveryState& State, int32 Suffix = 61)
	{
		FString Error;
		const TArray<FAuraRunRecoveryMember> Members = { Member(TEXT("player_a"), TEXT("Aura")) };
		return State.Initialize(true, FGuid(51, 2026, 9, Suffix), 1, TEXT("GameplayExpansionV1"),
			1, 0.0, 300.0, Members, Error);
	}

	const FAuraRunRecoveryMember* Find(const FAuraRunRecoveryState& State, FName MemberId)
	{
		for (const FAuraRunRecoveryMember& Candidate : State.GetMembers())
			if (Candidate.MemberId == MemberId) return &Candidate;
		return nullptr;
	}

	bool OpenPairRecovery(FAuraRunRecoveryState& State, int32& OutReceipt, FString& Error)
	{
		return State.RegisterDeath(true, TEXT("player_a"), 1, 1, 1.0, Error)
			== EAuraRunRecoveryResult::DeathAccepted
			&& State.BeginRecoveryChannel(true, TEXT("player_b"), TEXT("player_a"), 1.0, Error)
			== EAuraRunRecoveryResult::ChannelStarted
			&& State.TryReserveRecovery(true, TEXT("player_b"), TEXT("player_a"), 1, 4.0,
				OutReceipt, Error) == EAuraRunRecoveryResult::ChargeReserved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51DeathDispatchOnce,
	"Aura.Gameplay.Day51.DeathDispatchOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51DeathDispatchOnce::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	TestTrue(TEXT("Pair recovery initializes"), AuraGameplayDay51TestsPrivate::InitializePair(State));
	TestEqual(TEXT("First lethal dispatch creates recovery state"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 1, 1.0, Error), EAuraRunRecoveryResult::DeathAccepted);
	const FAuraRunRecoverySnapshot AfterDeath = State.BuildSnapshot();
	TestEqual(TEXT("Exactly one beacon exists"), AfterDeath.BeaconCount, 1);
	TestEqual(TEXT("Repeated lethal dispatch is idempotent"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 1, 1.1, Error), EAuraRunRecoveryResult::DuplicateDeath);
	TestTrue(TEXT("Duplicate death creates no revision or beacon"), State.BuildSnapshot() == AfterDeath);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51RescueRaceSingleCharge,
	"Aura.Gameplay.Day51.RescueRaceSingleCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51RescueRaceSingleCharge::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	int32 Receipt = 0;
	TestTrue(TEXT("Pair recovery initializes"), AuraGameplayDay51TestsPrivate::InitializePair(State));
	TestTrue(TEXT("Channel completion reserves one charge"), AuraGameplayDay51TestsPrivate::OpenPairRecovery(
		State, Receipt, Error));
	int32 DuplicateReceipt = 0;
	TestEqual(TEXT("Concurrent completion reuses the live receipt"), State.TryReserveRecovery(
		true, TEXT("player_b"), TEXT("player_a"), 1, 4.0, DuplicateReceipt, Error),
		EAuraRunRecoveryResult::DuplicateAccepted);
	TestEqual(TEXT("Race returns the original receipt"), DuplicateReceipt, Receipt);
	TestEqual(TEXT("Only one charge is reserved"), State.BuildSnapshot().ReservedCharges, 1);
	TestEqual(TEXT("Verified attachment commits recovery"), State.CommitRecoveryAttachment(
		true, Receipt, true, true, true, 4.1, Error), EAuraRunRecoveryResult::Recovered);
	TestEqual(TEXT("Exactly one shared charge is consumed"), State.BuildSnapshot().ChargesRemaining, 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51RecoverRetainsRunState,
	"Aura.Gameplay.Day51.RecoverRetainsRunState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51RecoverRetainsRunState::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	int32 Receipt = 0;
	TestTrue(TEXT("Solo recovery initializes"), AuraGameplayDay51TestsPrivate::InitializeSolo(State));
	const FAuraRunRecoveryRetainedState Before = State.GetMembers()[0].RetainedState;
	TestEqual(TEXT("Solo death starts checkpoint recovery"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 1, 1.0, Error), EAuraRunRecoveryResult::DeathAccepted);
	TestEqual(TEXT("Checkpoint reserves the solo charge after three seconds"), State.TryReserveRecovery(
		true, TEXT("Checkpoint"), TEXT("player_a"), 1, 4.0, Receipt, Error),
		EAuraRunRecoveryResult::ChargeReserved);
	TestEqual(TEXT("Verified solo attachment recovers"), State.CommitRecoveryAttachment(
		true, Receipt, true, true, true, 4.1, Error), EAuraRunRecoveryResult::Recovered);
	const FAuraRunRecoveryMember* Member = AuraGameplayDay51TestsPrivate::Find(State, TEXT("player_a"));
	TestNotNull(TEXT("Recovered member exists"), Member);
	if (Member)
	{
		TestTrue(TEXT("Ammo, augments, supplies and cooldowns are retained"), Member->RetainedState == Before);
		TestTrue(TEXT("Recovery publishes thirty-five percent health"), FMath::IsNearlyEqual(Member->HealthNormalized, 0.35f));
		TestTrue(TEXT("Recovery publishes fifty percent mana"), FMath::IsNearlyEqual(Member->ManaNormalized, 0.50f));
	}
	TestEqual(TEXT("Solo second death fails after its only charge was consumed"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 2, 5.0, Error), EAuraRunRecoveryResult::WipeFailed);
	TestEqual(TEXT("Solo failure cannot refund or duplicate the spent charge"),
		State.BuildSnapshot().ChargesRemaining, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51ReconnectDoesNotResurrect,
	"Aura.Gameplay.Day51.ReconnectDoesNotResurrect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51ReconnectDoesNotResurrect::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	AuraGameplayDay51TestsPrivate::InitializePair(State);
	State.RegisterDeath(true, TEXT("player_a"), 1, 1, 1.0, Error);
	State.SetConnected(true, TEXT("player_a"), false, 2.0, Error);
	State.SetConnected(true, TEXT("player_a"), true, 10.0, Error);
	const FAuraRunRecoveryMember* Member = AuraGameplayDay51TestsPrivate::Find(State, TEXT("player_a"));
	TestNotNull(TEXT("Reconnected member exists"), Member);
	if (Member)
	{
		TestEqual(TEXT("Dead reconnect remains AwaitingRescue"), Member->State,
			EAuraRunRecoveryMemberState::AwaitingRescue);
		TestTrue(TEXT("Reconnect does not heal"), FMath::IsNearlyZero(Member->HealthNormalized));
		TestEqual(TEXT("Reconnect creates no Alive publication"), State.BuildSnapshot().AlivePublicationCount, 0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51RecoveryAttachmentBeforeAlive,
	"Aura.Gameplay.Day51.RecoveryAttachmentBeforeAlive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51RecoveryAttachmentBeforeAlive::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	int32 Receipt = 0;
	AuraGameplayDay51TestsPrivate::InitializePair(State);
	TestTrue(TEXT("Recovery receipt opens"), AuraGameplayDay51TestsPrivate::OpenPairRecovery(State, Receipt, Error));
	TestEqual(TEXT("Failed first attachment schedules the one retry"), State.CommitRecoveryAttachment(
		true, Receipt, false, false, false, 4.1, Error), EAuraRunRecoveryResult::AttachmentRetryScheduled);
	TestEqual(TEXT("Retry cannot execute before one second"), State.CommitRecoveryAttachment(
		true, Receipt, false, false, false, 4.9, Error), EAuraRunRecoveryResult::Rejected);
	TestEqual(TEXT("Second failed attachment aborts technically"), State.CommitRecoveryAttachment(
		true, Receipt, false, false, false, 5.1, Error), EAuraRunRecoveryResult::TechnicalRecoveryFailure);
	const FAuraRunRecoverySnapshot Snapshot = State.BuildSnapshot();
	TestEqual(TEXT("Failed creation consumes no charge"), Snapshot.ChargesRemaining, 2);
	TestEqual(TEXT("Failed creation releases the reservation"), Snapshot.ReservedCharges, 0);
	TestEqual(TEXT("Alive is never published"), Snapshot.AlivePublicationCount, 0);
	TestEqual(TEXT("Legacy save is never invoked by the inert reducer"), Snapshot.LegacySaveCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51DisconnectRetainsCombatAndCooldowns,
	"Aura.Gameplay.Day51.DisconnectRetainsCombatAndCooldowns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51DisconnectRetainsCombatAndCooldowns::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	AuraGameplayDay51TestsPrivate::InitializePair(State);
	const FAuraRunRecoveryRetainedState Before = State.GetMembers()[0].RetainedState;
	State.SetConnected(true, TEXT("player_a"), false, 2.0, Error);
	const double OriginalExpiry = AuraGameplayDay51TestsPrivate::Find(State, TEXT("player_a"))->DisconnectLeaseExpiryServerTime;
	TestEqual(TEXT("Repeated disconnect is idempotent"), State.SetConnected(
		true, TEXT("player_a"), false, 20.0, Error), EAuraRunRecoveryResult::NoChange);
	const FAuraRunRecoveryMember* Proxy = AuraGameplayDay51TestsPrivate::Find(State, TEXT("player_a"));
	TestTrue(TEXT("Repeated disconnect cannot extend the lease"), Proxy && FMath::IsNearlyEqual(
		Proxy->DisconnectLeaseExpiryServerTime, OriginalExpiry));
	TestTrue(TEXT("Disconnect cannot reset ability, evade, supply or ammo state"), Proxy && Proxy->RetainedState == Before);
	TestEqual(TEXT("Committed damage may kill the targetable proxy"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 1, 21.0, Error), EAuraRunRecoveryResult::DeathAccepted);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51ProxyWipeAndExpiry,
	"Aura.Gameplay.Day51.ProxyWipeAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51ProxyWipeAndExpiry::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraRunRecoveryState ExpiryState;
	AuraGameplayDay51TestsPrivate::InitializePair(ExpiryState, 52);
	ExpiryState.RegisterDeath(true, TEXT("player_a"), 1, 1, 1.0, Error);
	ExpiryState.SetConnected(true, TEXT("player_b"), false, 2.0, Error);
	TestEqual(TEXT("Live proxy grace prevents early wipe"), ExpiryState.Tick(true, 61.9, Error),
		EAuraRunRecoveryResult::NoChange);
	TestEqual(TEXT("Committed proxy expiry terminates the run"), ExpiryState.Tick(true, 62.0, Error),
		EAuraRunRecoveryResult::WipeFailed);
	const FAuraRunRecoverySnapshot AfterExpiry = ExpiryState.BuildSnapshot();
	TestEqual(TEXT("Repeated expiry cannot fail twice"), ExpiryState.Tick(true, 63.0, Error),
		EAuraRunRecoveryResult::Rejected);
	TestTrue(TEXT("Repeated terminal tick cannot change the result"), ExpiryState.BuildSnapshot() == AfterExpiry);

	FAuraRunRecoveryState DeathState;
	AuraGameplayDay51TestsPrivate::InitializePair(DeathState, 53);
	DeathState.RegisterDeath(true, TEXT("player_a"), 1, 1, 1.0, Error);
	DeathState.SetConnected(true, TEXT("player_b"), false, 2.0, Error);
	TestEqual(TEXT("Accepted proxy death also terminates once"), DeathState.RegisterDeath(
		true, TEXT("player_b"), 1, 1, 3.0, Error), EAuraRunRecoveryResult::WipeFailed);
	TestEqual(TEXT("Wipe consumes no recovery charge"), DeathState.BuildSnapshot().ChargesRemaining, 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51TeamWipeFails,
	"Aura.Gameplay.Day51.TeamWipeFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51TeamWipeFails::RunTest(const FString& Parameters)
{
	FAuraRunRecoveryState State;
	FString Error;
	int32 Receipt = 0;
	AuraGameplayDay51TestsPrivate::InitializePair(State);
	TestEqual(TEXT("First death awaits rescue"), State.RegisterDeath(
		true, TEXT("player_a"), 1, 1, 1.0, Error), EAuraRunRecoveryResult::DeathAccepted);
	TestEqual(TEXT("Survivor starts the rescue channel"), State.BeginRecoveryChannel(
		true, TEXT("player_b"), TEXT("player_a"), 1.0, Error), EAuraRunRecoveryResult::ChannelStarted);
	TestEqual(TEXT("Channel completion reserves but does not spend one charge"), State.TryReserveRecovery(
		true, TEXT("player_b"), TEXT("player_a"), 1, 4.0, Receipt, Error),
		EAuraRunRecoveryResult::ChargeReserved);
	TestEqual(TEXT("Survivor death before attachment fails the team"), State.RegisterDeath(
		true, TEXT("player_b"), 1, 1, 4.1, Error), EAuraRunRecoveryResult::WipeFailed);
	const FAuraRunRecoverySnapshot Failed = State.BuildSnapshot();
	TestTrue(TEXT("Unused charges cannot auto-revive a team wipe"), Failed.bTerminal && Failed.ChargesRemaining == 2);
	TestEqual(TEXT("Team wipe releases the uncommitted charge"), Failed.ReservedCharges, 0);
	TestEqual(TEXT("Team wipe expires orphan beacons"), Failed.BeaconCount, 0);
	TestEqual(TEXT("Wipe reason is explicit"), Failed.TerminalReason, FName(TEXT("TeamWipe")));
	TestEqual(TEXT("Post-wipe death callback is rejected"), State.RegisterDeath(
		true, TEXT("player_b"), 1, 1, 4.1, Error), EAuraRunRecoveryResult::Rejected);
	TestTrue(TEXT("Team wipe is published exactly once"), State.BuildSnapshot() == Failed);

	FAuraRunRecoveryState SimultaneousState;
	AuraGameplayDay51TestsPrivate::InitializePair(SimultaneousState, 54);
	SimultaneousState.RegisterDeath(true, TEXT("player_a"), 1, 1, 5.0, Error);
	TestEqual(TEXT("Two deaths on the same server tick also fail once"), SimultaneousState.RegisterDeath(
		true, TEXT("player_b"), 1, 1, 5.0, Error), EAuraRunRecoveryResult::WipeFailed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGameplayDay51LegacyRecoveryUnchanged,
	"Aura.Gameplay.Day51.LegacyRecoveryUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay51LegacyRecoveryUnchanged::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Gameplay expansion profile opts into the isolated reducer"),
		FAuraRunRecoveryState::UsesCooperativeRecovery(TEXT("GameplayExpansionV1")));
	TestFalse(TEXT("Legacy profile does not opt into cooperative recovery"),
		FAuraRunRecoveryState::UsesCooperativeRecovery(TEXT("Legacy")));
	TestFalse(TEXT("Empty profile does not opt into cooperative recovery"),
		FAuraRunRecoveryState::UsesCooperativeRecovery(NAME_None));
	return !HasAnyErrors();
}

#endif
