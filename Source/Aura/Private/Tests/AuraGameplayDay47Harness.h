// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

#include "Gameplay/AuraMissionTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class EAuraDay47OfferResult : uint8
{
	Rejected,
	Granted,
	DuplicateBoundary,
	Selected,
	DuplicateAccepted,
	AutoSelected,
	ChoiceClosed,
	Expired,
	StaleRevision
};

/** Test-only description of the eight architecture-defined offers. */
struct FAuraDay47AugmentDefinition
{
	FName Id = NAME_None;
	FName Eligibility = NAME_None;
	FName ParameterId = NAME_None;
	double BaselineValue = 0.0;
	double ModifiedValue = 0.0;
	double MinimumValue = 0.0;
	double MaximumValue = 0.0;
};

/** Owner-scoped, value-only offer projection used by deterministic tests. */
struct FAuraDay47OfferSnapshot
{
	bool bOwnerAuthorized = false;
	FGuid RunId;
	int32 Epoch = 0;
	FName OwnerId = NAME_None;
	FName RoleId = NAME_None;
	int32 BoundaryIndex = 0;
	int32 OfferRevision = 0;
	double OfferDeadlineServerTime = 0.0;
	bool bChoiceOpen = false;
	bool bAutoSelected = false;
	FName LastChoiceRequestId = NAME_None;
	FName LastChoiceAugmentId = NAME_None;
	TArray<FName> Offers;
	TArray<FName> SelectedAugments;

	bool operator==(const FAuraDay47OfferSnapshot& Other) const;
	bool operator!=(const FAuraDay47OfferSnapshot& Other) const { return !(*this == Other); }
};

/**
 * Test-only Day 47 boundary owner. It consumes real Day 42/43 mission cell
 * completion results, but deliberately does not publish a runtime augment
 * component, config asset, UI offer, or modifier consumer.
 */
class FAuraDay47ContractHarness final
{
public:
	static const TArray<FAuraDay47AugmentDefinition>& GetDefinitions();
	static bool ValidateCatalog(FString& OutError);
	static const FAuraDay47AugmentDefinition* FindDefinition(FName AugmentId);

	bool Initialize(const FGuid& InRunId, int32 InEpoch, FName InOwnerId, FName InRoleId, int32 InSeed,
		FString& OutError);
	bool CompleteNextCell(double CompletionNow, FString& OutError);
	EAuraDay47OfferResult ConsumeCellCompletedBoundary(FName CellId, int32 CellGeneration,
		int32 CompletionSequence, double CompletionNow, FString& OutError);
	EAuraDay47OfferResult Choose(FName RequestId, int32 ExpectedOfferRevision, FName AugmentId,
		double Now, FString& OutError);
	EAuraDay47OfferResult ResolveChoiceDeadline(double Now, FString& OutError);
	bool Reconnect(FString& OutError) const;

	FAuraDay47OfferSnapshot BuildOwnerSnapshot(FName RequestingOwnerId) const;

	const FAuraMissionRunState& GetMissionState() const { return MissionState; }
	const FGuid& GetRunId() const { return RunId; }
	int32 GetEpoch() const { return Epoch; }
	FName GetOwnerId() const { return OwnerId; }
	FName GetRoleId() const { return RoleId; }
	int32 GetSeed() const { return Seed; }
	int32 GetGrantedBoundaryCount() const { return GrantedBoundaryCount; }
	bool IsInitialized() const { return bInitialized; }

private:
	static uint64 NextDeterministicValue(uint64& State);
	static FString MakeBoundaryKey(const FGuid& InRunId, int32 InEpoch, FName CellId,
		int32 CellGeneration, int32 CompletionSequence);
	static bool IsRoleValid(FName InRoleId);
	static bool IsEligible(const FAuraDay47AugmentDefinition& Definition, FName InRoleId);

	TArray<FName> GetEligibleDefinitionIds() const;
	bool GenerateOffers(double Now, FString& OutError);

	bool bInitialized = false;
	FGuid RunId;
	int32 Epoch = 0;
	FName OwnerId = NAME_None;
	FName RoleId = NAME_None;
	int32 Seed = 0;
	int32 OfferStreamCounter = 0;
	int32 GrantedBoundaryCount = 0;
	int32 OfferRevision = 0;
	int32 OfferBoundaryIndex = 0;
	double OfferDeadlineServerTime = 0.0;
	bool bChoiceOpen = false;
	bool bAutoSelected = false;
	FName LastChoiceRequestId = NAME_None;
	FName LastChoiceAugmentId = NAME_None;
	int32 LastChoiceRevision = 0;
	TArray<FName> CurrentOffers;
	TArray<FName> SelectedAugments;
	TSet<FString> ConsumedBoundaryKeys;
	TMap<FName, int32> CompletedCellGenerations;
	FAuraMissionRunState MissionState;
};

#endif
