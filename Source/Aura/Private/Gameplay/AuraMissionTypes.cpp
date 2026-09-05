// Copyright Druid Mechanics

#include "Gameplay/AuraMissionTypes.h"

namespace AuraMissionTypesPrivate
{
	void SetError(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}
}

bool FAuraMissionRunState::Touch(FString& OutError)
{
	if (Revision == MAX_int32)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("RevisionOverflow"));
		return false;
	}
	++Revision;
	return true;
}

bool FAuraMissionRunState::IsValidTransition(EAuraMissionPhase From, EAuraMissionPhase To) const
{
	return (From == EAuraMissionPhase::Hub && To == EAuraMissionPhase::Preparing)
		|| (From == EAuraMissionPhase::Preparing && (To == EAuraMissionPhase::Hub || To == EAuraMissionPhase::Active || To == EAuraMissionPhase::Failed))
		|| (From == EAuraMissionPhase::Active && (To == EAuraMissionPhase::Extraction || To == EAuraMissionPhase::Settling || To == EAuraMissionPhase::Failed || To == EAuraMissionPhase::Aborted))
		|| (From == EAuraMissionPhase::Extraction && (To == EAuraMissionPhase::Settling || To == EAuraMissionPhase::Failed || To == EAuraMissionPhase::Aborted))
		|| (From == EAuraMissionPhase::Settling && (To == EAuraMissionPhase::Completed || To == EAuraMissionPhase::Failed || To == EAuraMissionPhase::Aborted));
}

bool FAuraMissionRunState::TryBeginPreparation(const FGuid& InRunId, int32 InEpoch, FName InProfile,
	FName InMissionId, FName InArrangementId, double Now, double RunCapSeconds, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Hub)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("RunAlreadyInProgress"));
		return false;
	}
	if (!InRunId.IsValid() || InEpoch <= 0 || InProfile.IsNone() || InMissionId.IsNone() || InArrangementId.IsNone()
		|| !FMath::IsFinite(Now) || !FMath::IsFinite(RunCapSeconds) || RunCapSeconds <= 0.0)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("InvalidPreparationIdentity"));
		return false;
	}

	GameplayProfile = InProfile;
	MissionId = InMissionId;
	ArrangementId = InArrangementId;
	RunId = InRunId;
	Epoch = InEpoch;
	Revision = 0;
	Phase = EAuraMissionPhase::Preparing;
	PhaseDeadlineServerTime = Now + 30.0;
	RunDeadlineServerTime = Now + RunCapSeconds;
	CurrentCellIndex = INDEX_NONE;
	bObjectiveComplete = false;
	TerminalReason = NAME_None;
	Members.Reset();
	Cells.Reset();
	return Touch(OutError);
}

bool FAuraMissionRunState::ConfigureClearCells(const TArray<FAuraMissionCellState>& InCells, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Preparing || InCells.Num() == 0)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("CellsNotConfigurable"));
		return false;
	}
	for (const FAuraMissionCellState& Cell : InCells)
	{
		if (Cell.CellId.IsNone() || Cell.RequiredDeaths <= 0 || Cell.AcceptedDeaths != 0 || Cell.bCompleted
			|| Cell.ActiveGeneration <= 0 || Cell.AcceptedDeathKeys.Num() != 0)
		{
			AuraMissionTypesPrivate::SetError(OutError, TEXT("InvalidClearCell"));
			return false;
		}
	}
	Cells = InCells;
	CurrentCellIndex = 0;
	bObjectiveComplete = false;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryAdvanceCurrentCellGeneration(int32 NewGeneration, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Active || !Cells.IsValidIndex(CurrentCellIndex)
		|| NewGeneration <= 0 || NewGeneration <= Cells[CurrentCellIndex].ActiveGeneration
		|| Cells[CurrentCellIndex].ActiveGeneration == MAX_int32)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("CellGenerationAdvanceRejected"));
		return false;
	}
	Cells[CurrentCellIndex].ActiveGeneration = NewGeneration;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryActivate(double Now, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Preparing || !Cells.IsValidIndex(CurrentCellIndex)
		|| !FMath::IsFinite(Now) || Now >= PhaseDeadlineServerTime || Now >= RunDeadlineServerTime)
	{
		AuraMissionTypesPrivate::SetError(OutError, Phase == EAuraMissionPhase::Preparing ? TEXT("PreparationExpiredOrNotReady") : TEXT("InvalidActivation"));
		return false;
	}
	if (!IsValidTransition(Phase, EAuraMissionPhase::Active))
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("InvalidTransition"));
		return false;
	}
	Phase = EAuraMissionPhase::Active;
	PhaseDeadlineServerTime = RunDeadlineServerTime;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryBeginExtraction(double Now, double ExtractionSeconds, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Active || !bObjectiveComplete || !FMath::IsFinite(Now)
		|| !FMath::IsFinite(ExtractionSeconds) || ExtractionSeconds <= 0.0 || Now > RunDeadlineServerTime)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("ExtractionNotReady"));
		return false;
	}
	Phase = EAuraMissionPhase::Extraction;
	PhaseDeadlineServerTime = FMath::Min(RunDeadlineServerTime, Now + ExtractionSeconds);
	return Touch(OutError);
}

bool FAuraMissionRunState::TryBeginSettlement(bool bSuccess, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Extraction
		|| (bSuccess && !bObjectiveComplete))
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("SettlementNotReady"));
		return false;
	}
	Phase = EAuraMissionPhase::Settling;
	TerminalReason = bSuccess ? TEXT("Success") : TEXT("MissionFailed");
	PhaseDeadlineServerTime = 0.0;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryCommitSettlement(FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Settling || TerminalReason.IsNone())
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("SettlementNotOpen"));
		return false;
	}
	Phase = TerminalReason == TEXT("Success") ? EAuraMissionPhase::Completed : EAuraMissionPhase::Failed;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryCancelPreparation(FName Reason, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Preparing || Reason.IsNone())
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("PreparationNotCancelable"));
		return false;
	}
	Phase = EAuraMissionPhase::Hub;
	PhaseDeadlineServerTime = 0.0;
	RunDeadlineServerTime = 0.0;
	TerminalReason = Reason;
	CurrentCellIndex = INDEX_NONE;
	Cells.Reset();
	return Touch(OutError);
}

bool FAuraMissionRunState::TryFail(FName Reason, FString& OutError)
{
	OutError.Reset();
	if (IsTerminal() || Reason.IsNone() || !IsValidTransition(Phase, EAuraMissionPhase::Failed))
	{
		AuraMissionTypesPrivate::SetError(OutError, IsTerminal() ? TEXT("TerminalRun") : TEXT("InvalidFailure"));
		return false;
	}
	Phase = EAuraMissionPhase::Failed;
	TerminalReason = Reason;
	PhaseDeadlineServerTime = 0.0;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryAbort(FName Reason, FString& OutError)
{
	OutError.Reset();
	if (IsTerminal() || Reason.IsNone() || !IsValidTransition(Phase, EAuraMissionPhase::Aborted))
	{
		AuraMissionTypesPrivate::SetError(OutError, IsTerminal() ? TEXT("TerminalRun") : TEXT("InvalidAbort"));
		return false;
	}
	Phase = EAuraMissionPhase::Aborted;
	TerminalReason = Reason;
	PhaseDeadlineServerTime = 0.0;
	return Touch(OutError);
}

bool FAuraMissionRunState::TryCancelPreparationAt(double Now, FName Reason, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Preparing || !FMath::IsFinite(Now)
		|| Now < PhaseDeadlineServerTime || Reason.IsNone())
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("PreparationNotExpired"));
		return false;
	}
	return TryCancelPreparation(Reason, OutError);
}

EAuraMissionMutationResult FAuraMissionRunState::RegisterCellDeath(FName CellId, FName LeaseId, int32 Generation,
	int32 DeathSequence, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraMissionPhase::Active || !Cells.IsValidIndex(CurrentCellIndex)
		|| CellId != GetCurrentCellId() || LeaseId.IsNone() || Generation <= 0 || DeathSequence <= 0)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("DeathNotAdmissible"));
		return EAuraMissionMutationResult::Rejected;
	}
	if (Revision == MAX_int32)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("RevisionOverflow"));
		return EAuraMissionMutationResult::Rejected;
	}

	FAuraMissionCellState& Cell = Cells[CurrentCellIndex];
	if (Generation != Cell.ActiveGeneration)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("StaleCellGeneration"));
		return EAuraMissionMutationResult::Rejected;
	}
	FAuraMissionDeathKey Key;
	Key.LeaseId = LeaseId;
	Key.Epoch = Epoch;
	Key.Generation = Generation;
	Key.DeathSequence = DeathSequence;
	if (Cell.HasAcceptedDeath(Key))
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("DuplicateDeath"));
		return EAuraMissionMutationResult::Rejected;
	}
	if (Cell.bCompleted || Cell.AcceptedDeaths >= Cell.RequiredDeaths)
	{
		AuraMissionTypesPrivate::SetError(OutError, TEXT("CellAlreadyComplete"));
		return EAuraMissionMutationResult::Rejected;
	}

	Cell.AcceptedDeathKeys.Add(Key);
	++Cell.AcceptedDeaths;
	if (Cell.AcceptedDeaths == Cell.RequiredDeaths)
	{
		Cell.bCompleted = true;
		if (Cells.IsValidIndex(CurrentCellIndex + 1))
		{
			++CurrentCellIndex;
			Touch(OutError);
			return EAuraMissionMutationResult::CellCompleted;
		}
		else
		{
			CurrentCellIndex = INDEX_NONE;
			bObjectiveComplete = true;
			Touch(OutError);
			return EAuraMissionMutationResult::ObjectiveCompleted;
		}
	}
	if (!Touch(OutError)) return EAuraMissionMutationResult::Rejected;
	return EAuraMissionMutationResult::Accepted;
}

bool FAuraMissionRunState::TryRegisterCellDeath(FName CellId, FName LeaseId, int32 Generation, int32 DeathSequence,
	FString& OutError)
{
	return RegisterCellDeath(CellId, LeaseId, Generation, DeathSequence, OutError) != EAuraMissionMutationResult::Rejected;
}

FAuraMissionPublicSnapshot FAuraMissionRunState::BuildPublicSnapshot() const
{
	FAuraMissionPublicSnapshot Snapshot;
	Snapshot.GameplayProfile = GameplayProfile;
	Snapshot.MissionId = MissionId;
	Snapshot.ArrangementId = ArrangementId;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.Revision = Revision;
	Snapshot.Phase = Phase;
	Snapshot.PhaseDeadlineServerTime = PhaseDeadlineServerTime;
	Snapshot.RunDeadlineServerTime = RunDeadlineServerTime;
	Snapshot.CurrentCellIndex = CurrentCellIndex;
	Snapshot.CurrentCellId = GetCurrentCellId();
	Snapshot.bObjectiveComplete = bObjectiveComplete;
	Snapshot.TerminalReason = TerminalReason;
	Snapshot.Members = Members;
	for (const FAuraMissionCellState& Cell : Cells)
	{
		if (Cell.bCompleted) ++Snapshot.CompletedCellCount;
	}
	if (Cells.IsValidIndex(CurrentCellIndex))
	{
		Snapshot.CurrentCellAcceptedDeaths = Cells[CurrentCellIndex].AcceptedDeaths;
		Snapshot.CurrentCellRequiredDeaths = Cells[CurrentCellIndex].RequiredDeaths;
	}
	return Snapshot;
}
