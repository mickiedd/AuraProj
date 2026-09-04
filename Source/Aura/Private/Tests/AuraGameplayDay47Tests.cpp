// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AuraGameplayDay47Harness.h"

namespace AuraGameplayDay47TestsPrivate
{
	FGuid TestRunId()
	{
		return FGuid(47, 2026, 9, 3);
	}

	bool Initialize(FAuraDay47ContractHarness& Harness, FName OwnerId = TEXT("player_a"),
		FName RoleId = TEXT("Aura"), int32 Seed = 41001)
	{
		FString Error;
		return Harness.Initialize(TestRunId(), 1, OwnerId, RoleId, Seed, Error);
	}

	bool ContainsAll(const TArray<FName>& Values, const TArray<FName>& Expected)
	{
		for (const FName Value : Expected)
		{
			if (!Values.Contains(Value)) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47DefinitionCatalog,
	"Aura.Gameplay.Day47A.DefinitionCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47DefinitionCatalog::RunTest(const FString& Parameters)
{
	FString Error;
	TestTrue(TEXT("The test-only eight-entry catalog validates"), FAuraDay47ContractHarness::ValidateCatalog(Error));
	const TArray<FAuraDay47AugmentDefinition>& Definitions = FAuraDay47ContractHarness::GetDefinitions();
	TestEqual(TEXT("Exactly eight architecture-defined augments exist"), Definitions.Num(), 8);
	TestNotNull(TEXT("field_medic is defined"), FAuraDay47ContractHarness::FindDefinition(TEXT("field_medic")));
	TestNotNull(TEXT("steady_hands is defined"), FAuraDay47ContractHarness::FindDefinition(TEXT("steady_hands")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47OfferEligibilityAndUniqueness,
	"Aura.Gameplay.Day47A.OfferEligibilityAndUniqueness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47OfferEligibilityAndUniqueness::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Aura offer harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness));
	FString Error;
	TestTrue(TEXT("The first real mission cell completion grants an offer"), Harness.CompleteNextCell(100.0, Error));
	const FAuraDay47OfferSnapshot Snapshot = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestTrue(TEXT("The owner can read the private offer snapshot"), Snapshot.bOwnerAuthorized);
	TestEqual(TEXT("One boundary exposes exactly three entries"), Snapshot.Offers.Num(), 3);
	TestEqual(TEXT("The first offer revision is one"), Snapshot.OfferRevision, 1);
	TestTrue(TEXT("The offer deadline is exactly twenty seconds after the event"),
		FMath::IsNearlyEqual(Snapshot.OfferDeadlineServerTime, 120.0));
	TSet<FName> UniqueOffers;
	for (const FName OfferId : Snapshot.Offers) UniqueOffers.Add(OfferId);
	TestEqual(TEXT("The offer entries are distinct"), UniqueOffers.Num(), Snapshot.Offers.Num());
	for (const FName OfferId : Snapshot.Offers)
	{
		const FAuraDay47AugmentDefinition* Definition = FAuraDay47ContractHarness::FindDefinition(OfferId);
		TestNotNull(TEXT("Every listed entry has a definition"), Definition);
		if (Definition)
		{
			TestTrue(TEXT("Every Aura offer is shared or Aura-eligible"), Definition->Eligibility == TEXT("Shared")
				|| Definition->Eligibility == TEXT("Aura"));
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47DeterministicSeedReplay,
	"Aura.Gameplay.Day47A.DeterministicSeedReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47DeterministicSeedReplay::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness First;
	FAuraDay47ContractHarness Replay;
	TestTrue(TEXT("First seeded harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(First));
	TestTrue(TEXT("Replay seeded harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Replay));
	FString Error;
	TestTrue(TEXT("First seeded cell completion succeeds"), First.CompleteNextCell(100.0, Error));
	TestTrue(TEXT("Replay seeded cell completion succeeds"), Replay.CompleteNextCell(100.0, Error));
	TestTrue(TEXT("Identical seed and authority input reproduce the same offer"),
		First.BuildOwnerSnapshot(TEXT("player_a")) == Replay.BuildOwnerSnapshot(TEXT("player_a")));
	const FAuraDay47OfferSnapshot BeforeChoice = First.BuildOwnerSnapshot(TEXT("player_a"));
	TestEqual(TEXT("First choice commits one listed augment"), First.Choose(TEXT("choice_1"), BeforeChoice.OfferRevision,
		BeforeChoice.Offers[0], 101.0, Error), EAuraDay47OfferResult::Selected);
	TestEqual(TEXT("Replay choice commits the same listed augment"), Replay.Choose(TEXT("choice_1"),
		BeforeChoice.OfferRevision, BeforeChoice.Offers[0], 101.0, Error), EAuraDay47OfferResult::Selected);
	TestTrue(TEXT("Choice replay remains value-identical"),
		First.BuildOwnerSnapshot(TEXT("player_a")) == Replay.BuildOwnerSnapshot(TEXT("player_a")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47BoundaryOffersExactlyTwo,
	"Aura.Gameplay.Day47A.BoundaryOffersExactlyTwo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47BoundaryOffersExactlyTwo::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Boundary harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness));
	FString Error;
	TestTrue(TEXT("First cell grants the first offer"), Harness.CompleteNextCell(100.0, Error));
	TestEqual(TEXT("An uncompleted second boundary cannot replace an open choice"), Harness.ConsumeCellCompletedBoundary(
		TEXT("assault_cell_2"), 1, 3, 101.0, Error), EAuraDay47OfferResult::Rejected);
	TestEqual(TEXT("Duplicate first boundary is ignored"), Harness.ConsumeCellCompletedBoundary(TEXT("assault_cell_1"),
		1, 3, 101.0, Error), EAuraDay47OfferResult::DuplicateBoundary);
	const FAuraDay47OfferSnapshot FirstOffer = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestEqual(TEXT("Selecting the first offer closes its choice window"), Harness.Choose(TEXT("choice_1"),
		FirstOffer.OfferRevision, FirstOffer.Offers[0], 102.0, Error), EAuraDay47OfferResult::Selected);
	TestTrue(TEXT("Second cell grants the second offer"), Harness.CompleteNextCell(200.0, Error));
	TestEqual(TEXT("Exactly two boundaries have granted offers"), Harness.GetGrantedBoundaryCount(), 2);
	const FAuraDay47OfferSnapshot SecondOffer = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestEqual(TEXT("Second boundary increments the offer revision once"), SecondOffer.OfferRevision, 2);
	TestEqual(TEXT("Duplicate second boundary is ignored"), Harness.ConsumeCellCompletedBoundary(TEXT("assault_cell_2"),
		1, 3, 201.0, Error), EAuraDay47OfferResult::DuplicateBoundary);
	TestTrue(TEXT("Mission objective is complete after the real second cell"), Harness.GetMissionState().bObjectiveComplete);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47ChoiceReplayNoStack,
	"Aura.Gameplay.Day47A.ChoiceReplayNoStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47ChoiceReplayNoStack::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Choice harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness));
	FString Error;
	TestTrue(TEXT("Choice offer is granted"), Harness.CompleteNextCell(100.0, Error));
	const FAuraDay47OfferSnapshot Offer = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestEqual(TEXT("Initial choice succeeds"), Harness.Choose(TEXT("choice_1"), Offer.OfferRevision, Offer.Offers[1],
		101.0, Error), EAuraDay47OfferResult::Selected);
	TestEqual(TEXT("Exact duplicate request returns its committed result"), Harness.Choose(TEXT("choice_1"),
		Offer.OfferRevision, Offer.Offers[1], 101.1, Error), EAuraDay47OfferResult::DuplicateAccepted);
	TestEqual(TEXT("A second request cannot stack another augment"), Harness.Choose(TEXT("choice_2"), Offer.OfferRevision,
		Offer.Offers[2], 101.2, Error), EAuraDay47OfferResult::ChoiceClosed);
	TestEqual(TEXT("One choice is retained"), Harness.BuildOwnerSnapshot(TEXT("player_a")).SelectedAugments.Num(), 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47TimeoutChoosesVisibleDefault,
	"Aura.Gameplay.Day47A.TimeoutChoosesVisibleDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47TimeoutChoosesVisibleDefault::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Timeout harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness));
	FString Error;
	TestTrue(TEXT("Timeout offer is granted"), Harness.CompleteNextCell(100.0, Error));
	const FAuraDay47OfferSnapshot Offer = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestEqual(TEXT("Deadline selects the first still-visible entry"), Harness.ResolveChoiceDeadline(
		Offer.OfferDeadlineServerTime, Error), EAuraDay47OfferResult::AutoSelected);
	const FAuraDay47OfferSnapshot After = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestFalse(TEXT("The timed-out choice window is closed"), After.bChoiceOpen);
	TestTrue(TEXT("The selected entry is the first listed entry"), After.SelectedAugments.Num() == 1
		&& After.SelectedAugments[0] == Offer.Offers[0]);
	TestTrue(TEXT("The receipt identifies an auto-choice"), After.bAutoSelected);
	TestEqual(TEXT("Late selection cannot add another choice"), Harness.Choose(TEXT("late_choice"), Offer.OfferRevision,
		Offer.Offers[1], Offer.OfferDeadlineServerTime + 0.1, Error), EAuraDay47OfferResult::ChoiceClosed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47ReconnectKeepsOffer,
	"Aura.Gameplay.Day47A.ReconnectKeepsOffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47ReconnectKeepsOffer::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Reconnect harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness));
	FString Error;
	TestTrue(TEXT("Reconnect offer is granted"), Harness.CompleteNextCell(100.0, Error));
	const FAuraDay47OfferSnapshot Before = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestTrue(TEXT("Same-server reconnect succeeds without a reroll"), Harness.Reconnect(Error));
	const FAuraDay47OfferSnapshot After = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	TestTrue(TEXT("Reconnect preserves offers and their deadline"), Before == After);
	TestEqual(TEXT("Reconnect does not add a selection"), After.SelectedAugments.Num(), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay47OwnerPrivacyAndStaleBoundary,
	"Aura.Gameplay.Day47A.OwnerPrivacyAndStaleBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay47OwnerPrivacyAndStaleBoundary::RunTest(const FString& Parameters)
{
	FAuraDay47ContractHarness Harness;
	TestTrue(TEXT("Privacy harness initializes"), AuraGameplayDay47TestsPrivate::Initialize(Harness, TEXT("player_a"),
		TEXT("BungeeMan"), 41002));
	FString Error;
	TestTrue(TEXT("Privacy offer is granted"), Harness.CompleteNextCell(100.0, Error));
	const FAuraDay47OfferSnapshot OwnerSnapshot = Harness.BuildOwnerSnapshot(TEXT("player_a"));
	const FAuraDay47OfferSnapshot ForeignSnapshot = Harness.BuildOwnerSnapshot(TEXT("player_b"));
	TestTrue(TEXT("Owner snapshot is authorized"), OwnerSnapshot.bOwnerAuthorized);
	TestFalse(TEXT("Foreign owner cannot read offers"), ForeignSnapshot.bOwnerAuthorized);
	TestEqual(TEXT("Foreign snapshot exposes no offer IDs"), ForeignSnapshot.Offers.Num(), 0);
	TestEqual(TEXT("A future cell generation cannot mint an offer"), Harness.ConsumeCellCompletedBoundary(
		TEXT("assault_cell_1"), 2, 3, 101.0, Error), EAuraDay47OfferResult::Rejected);
	TestEqual(TEXT("A wrong completion sequence cannot mint an offer"), Harness.ConsumeCellCompletedBoundary(
		TEXT("assault_cell_1"), 1, 2, 101.0, Error), EAuraDay47OfferResult::Rejected);
	TestEqual(TEXT("Stale boundary attempts leave one offer revision"), Harness.BuildOwnerSnapshot(TEXT("player_a")).OfferRevision,
		OwnerSnapshot.OfferRevision);
	return !HasAnyErrors();
}

#endif
