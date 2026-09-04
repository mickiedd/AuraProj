// Copyright Druid Mechanics

#include "AuraGameplayDay47Harness.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AuraGameplayDay47HarnessPrivate
{
	void SetError(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	void SetFormattedError(FString& OutError, const TCHAR* Context, const FString& Detail)
	{
		OutError = FString::Printf(TEXT("%s:%s"), Context, *Detail);
	}
}

bool FAuraDay47OfferSnapshot::operator==(const FAuraDay47OfferSnapshot& Other) const
{
	return bOwnerAuthorized == Other.bOwnerAuthorized && RunId == Other.RunId && Epoch == Other.Epoch
		&& OwnerId == Other.OwnerId && RoleId == Other.RoleId && BoundaryIndex == Other.BoundaryIndex
		&& OfferRevision == Other.OfferRevision && FMath::IsNearlyEqual(OfferDeadlineServerTime,
			Other.OfferDeadlineServerTime) && bChoiceOpen == Other.bChoiceOpen
		&& bAutoSelected == Other.bAutoSelected && LastChoiceRequestId == Other.LastChoiceRequestId
		&& LastChoiceAugmentId == Other.LastChoiceAugmentId && Offers == Other.Offers
		&& SelectedAugments == Other.SelectedAugments;
}

const TArray<FAuraDay47AugmentDefinition>& FAuraDay47ContractHarness::GetDefinitions()
{
	static TArray<FAuraDay47AugmentDefinition> Definitions;
	if (Definitions.IsEmpty())
	{
		const auto Add = [](const TCHAR* Id, const TCHAR* Eligibility, const TCHAR* ParameterId,
			double BaselineValue, double ModifiedValue, double MinimumValue, double MaximumValue)
		{
			FAuraDay47AugmentDefinition Definition;
			Definition.Id = Id;
			Definition.Eligibility = Eligibility;
			Definition.ParameterId = ParameterId;
			Definition.BaselineValue = BaselineValue;
			Definition.ModifiedValue = ModifiedValue;
			Definition.MinimumValue = MinimumValue;
			Definition.MaximumValue = MaximumValue;
			Definitions.Add(Definition);
		};
		Add(TEXT("quick_step"), TEXT("Shared"), TEXT("EvadeCooldownSeconds"), 2.5, 2.0, 2.0, 2.5);
		Add(TEXT("guarded_step"), TEXT("Shared"), TEXT("PostEvadeDamageReductionFraction"), 0.0, 0.2, 0.0, 0.2);
		Add(TEXT("steady_hands"), TEXT("Shared"), TEXT("ExposedDurationSeconds"), 3.0, 4.0, 3.0, 4.0);
		Add(TEXT("field_medic"), TEXT("Shared"), TEXT("MedkitHealFraction"), 0.35, 0.45, 0.35, 0.45);
		Add(TEXT("forked_spark"), TEXT("Aura"), TEXT("ElectrocuteAdditionalTargetCount"), 0.0, 1.0, 0.0, 1.0);
		Add(TEXT("ember_ring"), TEXT("Aura"), TEXT("FireBlastRadiusScale"), 1.0, 1.2, 1.0, 1.2);
		Add(TEXT("fast_cycle"), TEXT("BungeeMan"), TEXT("ReloadSeconds"), 1.25, 1.0, 1.0, 1.25);
		Add(TEXT("wide_trap"), TEXT("BungeeMan"), TEXT("ShockTrapRadius"), 250.0, 325.0, 250.0, 325.0);
	}
	return Definitions;
}

bool FAuraDay47ContractHarness::ValidateCatalog(FString& OutError)
{
	OutError.Reset();
	const TArray<FAuraDay47AugmentDefinition>& Definitions = GetDefinitions();
	if (Definitions.Num() != 8)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("CatalogCountInvalid"));
		return false;
	}

	const TSet<FName> ExpectedIds = {
		TEXT("quick_step"), TEXT("guarded_step"), TEXT("steady_hands"), TEXT("field_medic"),
		TEXT("forked_spark"), TEXT("ember_ring"), TEXT("fast_cycle"), TEXT("wide_trap")};
	TSet<FName> SeenIds;
	int32 SharedCount = 0;
	int32 AuraCount = 0;
	int32 BungeeManCount = 0;
	for (const FAuraDay47AugmentDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone() || Definition.ParameterId.IsNone() || !ExpectedIds.Contains(Definition.Id)
			|| SeenIds.Contains(Definition.Id) || !FMath::IsFinite(Definition.BaselineValue)
			|| !FMath::IsFinite(Definition.ModifiedValue) || !FMath::IsFinite(Definition.MinimumValue)
			|| !FMath::IsFinite(Definition.MaximumValue) || Definition.MinimumValue > Definition.MaximumValue
			|| Definition.BaselineValue < Definition.MinimumValue || Definition.BaselineValue > Definition.MaximumValue
			|| Definition.ModifiedValue < Definition.MinimumValue || Definition.ModifiedValue > Definition.MaximumValue)
		{
			AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("CatalogEntryInvalid"));
			return false;
		}
		SeenIds.Add(Definition.Id);
		if (Definition.Eligibility == TEXT("Shared")) ++SharedCount;
		else if (Definition.Eligibility == TEXT("Aura")) ++AuraCount;
		else if (Definition.Eligibility == TEXT("BungeeMan")) ++BungeeManCount;
		else
		{
			AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("CatalogEligibilityInvalid"));
			return false;
		}
	}
	if (SeenIds.Num() != ExpectedIds.Num() || SharedCount != 4 || AuraCount != 2 || BungeeManCount != 2)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("CatalogEligibilityCountsInvalid"));
		return false;
	}
	return true;
}

const FAuraDay47AugmentDefinition* FAuraDay47ContractHarness::FindDefinition(FName AugmentId)
{
	for (const FAuraDay47AugmentDefinition& Definition : GetDefinitions())
	{
		if (Definition.Id == AugmentId) return &Definition;
	}
	return nullptr;
}

bool FAuraDay47ContractHarness::IsRoleValid(FName InRoleId)
{
	return InRoleId == TEXT("Aura") || InRoleId == TEXT("BungeeMan");
}

bool FAuraDay47ContractHarness::IsEligible(const FAuraDay47AugmentDefinition& Definition, FName InRoleId)
{
	return Definition.Eligibility == TEXT("Shared") || Definition.Eligibility == InRoleId;
}

bool FAuraDay47ContractHarness::Initialize(const FGuid& InRunId, int32 InEpoch, FName InOwnerId, FName InRoleId,
	int32 InSeed, FString& OutError)
{
	OutError.Reset();
	bInitialized = false;
	if (!ValidateCatalog(OutError) || !InRunId.IsValid() || InEpoch <= 0 || InOwnerId.IsNone()
		|| !IsRoleValid(InRoleId) || InSeed <= 0)
	{
		if (OutError.IsEmpty()) AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("InvalidOfferOwner"));
		return false;
	}

	RunId = InRunId;
	Epoch = InEpoch;
	OwnerId = InOwnerId;
	RoleId = InRoleId;
	Seed = InSeed;
	OfferStreamCounter = 0;
	GrantedBoundaryCount = 0;
	OfferRevision = 0;
	OfferBoundaryIndex = 0;
	OfferDeadlineServerTime = 0.0;
	bChoiceOpen = false;
	bAutoSelected = false;
	LastChoiceRequestId = NAME_None;
	LastChoiceAugmentId = NAME_None;
	LastChoiceRevision = 0;
	CurrentOffers.Reset();
	SelectedAugments.Reset();
	ConsumedBoundaryKeys.Reset();
	CompletedCellGenerations.Reset();
	MissionState = FAuraMissionRunState();

	if (!MissionState.TryBeginPreparation(RunId, Epoch, TEXT("GameplayExpansionV1"), TEXT("Assault"),
		TEXT("arrangement_a"), 10.0, 1200.0, OutError))
	{
		return false;
	}
	TArray<FAuraMissionCellState> Cells;
	FAuraMissionCellState& FirstCell = Cells.AddDefaulted_GetRef();
	FirstCell.CellId = TEXT("assault_cell_1");
	FirstCell.RequiredDeaths = 3;
	FAuraMissionCellState& SecondCell = Cells.AddDefaulted_GetRef();
	SecondCell.CellId = TEXT("assault_cell_2");
	SecondCell.RequiredDeaths = 3;
	if (!MissionState.ConfigureClearCells(Cells, OutError) || !MissionState.TryActivate(11.0, OutError))
	{
		return false;
	}
	bInitialized = true;
	return true;
}

bool FAuraDay47ContractHarness::CompleteNextCell(double CompletionNow, FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized() || !FMath::IsFinite(CompletionNow))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("HarnessNotReady"));
		return false;
	}
	if (bChoiceOpen)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("ChoiceWindowOpen"));
		return false;
	}
	if (!MissionState.Cells.IsValidIndex(MissionState.CurrentCellIndex))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("NoCurrentCell"));
		return false;
	}

	const FAuraMissionCellState Cell = MissionState.Cells[MissionState.CurrentCellIndex];
	EAuraMissionMutationResult LastResult = EAuraMissionMutationResult::Rejected;
	for (int32 DeathSequence = 1; DeathSequence <= Cell.RequiredDeaths; ++DeathSequence)
	{
		const FName LeaseId(*FString::Printf(TEXT("%s_lease_%d"), *Cell.CellId.ToString(), DeathSequence));
		FString Error;
		LastResult = MissionState.RegisterCellDeath(Cell.CellId, LeaseId, 1, DeathSequence, Error);
		const bool bExpectedIntermediate = DeathSequence < Cell.RequiredDeaths
			&& LastResult == EAuraMissionMutationResult::Accepted;
		const bool bExpectedTerminal = DeathSequence == Cell.RequiredDeaths
			&& (LastResult == EAuraMissionMutationResult::CellCompleted
				|| LastResult == EAuraMissionMutationResult::ObjectiveCompleted);
		if (!bExpectedIntermediate && !bExpectedTerminal)
		{
			AuraGameplayDay47HarnessPrivate::SetFormattedError(OutError, TEXT("MissionCellCompletionRejected"), Error);
			return false;
		}
	}

	CompletedCellGenerations.Add(Cell.CellId, 1);
	return ConsumeCellCompletedBoundary(Cell.CellId, 1, Cell.RequiredDeaths, CompletionNow, OutError)
		== EAuraDay47OfferResult::Granted;
}

EAuraDay47OfferResult FAuraDay47ContractHarness::ConsumeCellCompletedBoundary(FName CellId, int32 CellGeneration,
	int32 CompletionSequence, double CompletionNow, FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized() || CellId.IsNone() || CellGeneration <= 0 || CompletionSequence <= 0
		|| !FMath::IsFinite(CompletionNow))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("BoundaryIdentityInvalid"));
		return EAuraDay47OfferResult::Rejected;
	}
	const FString BoundaryKey = MakeBoundaryKey(RunId, Epoch, CellId, CellGeneration, CompletionSequence);
	if (ConsumedBoundaryKeys.Contains(BoundaryKey))
	{
		return EAuraDay47OfferResult::DuplicateBoundary;
	}
	const int32* ExpectedGeneration = CompletedCellGenerations.Find(CellId);
	const FAuraMissionCellState* CompletedCell = nullptr;
	for (const FAuraMissionCellState& Cell : MissionState.Cells)
	{
		if (Cell.CellId == CellId)
		{
			CompletedCell = &Cell;
			break;
		}
	}
	if (!ExpectedGeneration || *ExpectedGeneration != CellGeneration || !CompletedCell
		|| !CompletedCell->bCompleted || CompletedCell->AcceptedDeaths != CompletedCell->RequiredDeaths
		|| CompletionSequence != CompletedCell->RequiredDeaths || GrantedBoundaryCount >= 2)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("StaleOrInvalidBoundary"));
		return EAuraDay47OfferResult::Rejected;
	}
	if (bChoiceOpen)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("ChoiceWindowOpen"));
		return EAuraDay47OfferResult::ChoiceClosed;
	}
	ConsumedBoundaryKeys.Add(BoundaryKey);
	++GrantedBoundaryCount;
	if (!GenerateOffers(CompletionNow, OutError))
	{
		ConsumedBoundaryKeys.Remove(BoundaryKey);
		--GrantedBoundaryCount;
		return EAuraDay47OfferResult::Rejected;
	}
	return EAuraDay47OfferResult::Granted;
}

EAuraDay47OfferResult FAuraDay47ContractHarness::Choose(FName RequestId, int32 ExpectedOfferRevision, FName AugmentId,
	double Now, FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized() || RequestId.IsNone() || ExpectedOfferRevision <= 0 || AugmentId.IsNone()
		|| !FMath::IsFinite(Now))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("ChoiceRequestInvalid"));
		return EAuraDay47OfferResult::Rejected;
	}
	if (!bChoiceOpen)
	{
		if (RequestId == LastChoiceRequestId && ExpectedOfferRevision == LastChoiceRevision
			&& AugmentId == LastChoiceAugmentId)
		{
			return EAuraDay47OfferResult::DuplicateAccepted;
		}
		return EAuraDay47OfferResult::ChoiceClosed;
	}
	if (Now >= OfferDeadlineServerTime)
	{
		FString DeadlineError;
		ResolveChoiceDeadline(Now, DeadlineError);
		OutError = TEXT("ChoiceDeadlineCommitted");
		return EAuraDay47OfferResult::Expired;
	}
	if (ExpectedOfferRevision != OfferRevision)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("OfferRevisionStale"));
		return EAuraDay47OfferResult::StaleRevision;
	}
	if (!CurrentOffers.Contains(AugmentId))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("OfferNotListed"));
		return EAuraDay47OfferResult::Rejected;
	}
	SelectedAugments.Add(AugmentId);
	LastChoiceRequestId = RequestId;
	LastChoiceAugmentId = AugmentId;
	LastChoiceRevision = OfferRevision;
	bChoiceOpen = false;
	bAutoSelected = false;
	return EAuraDay47OfferResult::Selected;
}

EAuraDay47OfferResult FAuraDay47ContractHarness::ResolveChoiceDeadline(double Now, FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized() || !FMath::IsFinite(Now))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("DeadlineRequestInvalid"));
		return EAuraDay47OfferResult::Rejected;
	}
	if (!bChoiceOpen) return EAuraDay47OfferResult::ChoiceClosed;
	if (Now < OfferDeadlineServerTime)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("DeadlineNotReached"));
		return EAuraDay47OfferResult::Rejected;
	}
	if (CurrentOffers.Num() == 0)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("NoVisibleOffer"));
		return EAuraDay47OfferResult::Rejected;
	}
	SelectedAugments.Add(CurrentOffers[0]);
	LastChoiceRequestId = FName(*FString::Printf(TEXT("auto_choice_%d"), OfferBoundaryIndex));
	LastChoiceAugmentId = CurrentOffers[0];
	LastChoiceRevision = OfferRevision;
	bChoiceOpen = false;
	bAutoSelected = true;
	return EAuraDay47OfferResult::AutoSelected;
}

bool FAuraDay47ContractHarness::Reconnect(FString& OutError) const
{
	OutError.Reset();
	if (!IsInitialized())
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("ReconnectWithoutRun"));
		return false;
	}
	return true;
}

FAuraDay47OfferSnapshot FAuraDay47ContractHarness::BuildOwnerSnapshot(FName RequestingOwnerId) const
{
	FAuraDay47OfferSnapshot Snapshot;
	if (RequestingOwnerId != OwnerId || !IsInitialized()) return Snapshot;
	Snapshot.bOwnerAuthorized = true;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.OwnerId = OwnerId;
	Snapshot.RoleId = RoleId;
	Snapshot.BoundaryIndex = OfferBoundaryIndex;
	Snapshot.OfferRevision = OfferRevision;
	Snapshot.OfferDeadlineServerTime = OfferDeadlineServerTime;
	Snapshot.bChoiceOpen = bChoiceOpen;
	Snapshot.bAutoSelected = bAutoSelected;
	Snapshot.LastChoiceRequestId = LastChoiceRequestId;
	Snapshot.LastChoiceAugmentId = LastChoiceAugmentId;
	Snapshot.Offers = CurrentOffers;
	Snapshot.SelectedAugments = SelectedAugments;
	return Snapshot;
}

TArray<FName> FAuraDay47ContractHarness::GetEligibleDefinitionIds() const
{
	TArray<FName> Result;
	for (const FAuraDay47AugmentDefinition& Definition : GetDefinitions())
	{
		if (IsEligible(Definition, RoleId)) Result.Add(Definition.Id);
	}
	Result.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	return Result;
}

bool FAuraDay47ContractHarness::GenerateOffers(double Now, FString& OutError)
{
	OutError.Reset();
	if (OfferRevision == MAX_int32 || !FMath::IsFinite(Now))
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("OfferRevisionOverflow"));
		return false;
	}
	TArray<FName> Candidates = GetEligibleDefinitionIds();
	Candidates.RemoveAll([this](FName AugmentId)
	{
		return SelectedAugments.Contains(AugmentId);
	});
	if (Candidates.Num() < 3)
	{
		AuraGameplayDay47HarnessPrivate::SetError(OutError, TEXT("EligibleOfferPoolTooSmall"));
		return false;
	}

	uint64 State = (static_cast<uint64>(static_cast<uint32>(Seed)) << 32)
		^ static_cast<uint64>(++OfferStreamCounter);
	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = static_cast<int32>(NextDeterministicValue(State)
			% static_cast<uint64>(Index + 1));
		Candidates.Swap(Index, SwapIndex);
	}
	CurrentOffers.Reset();
	for (int32 Index = 0; Index < 3; ++Index) CurrentOffers.Add(Candidates[Index]);
	++OfferRevision;
	OfferBoundaryIndex = GrantedBoundaryCount;
	OfferDeadlineServerTime = Now + 20.0;
	bChoiceOpen = true;
	bAutoSelected = false;
	LastChoiceRequestId = NAME_None;
	LastChoiceAugmentId = NAME_None;
	LastChoiceRevision = 0;
	return true;
}

uint64 FAuraDay47ContractHarness::NextDeterministicValue(uint64& State)
{
	State += 0x9E3779B97F4A7C15ull;
	uint64 Value = State;
	Value = (Value ^ (Value >> 30)) * 0xBF58476D1CE4E5B9ull;
	Value = (Value ^ (Value >> 27)) * 0x94D049BB133111EBull;
	return Value ^ (Value >> 31);
}

FString FAuraDay47ContractHarness::MakeBoundaryKey(const FGuid& InRunId, int32 InEpoch, FName CellId,
	int32 CellGeneration, int32 CompletionSequence)
{
	return FString::Printf(TEXT("%s|%d|%s|%d|%d"), *InRunId.ToString(), InEpoch, *CellId.ToString(),
		CellGeneration, CompletionSequence);
}

#endif
