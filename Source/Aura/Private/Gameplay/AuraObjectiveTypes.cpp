// Copyright Druid Mechanics

#include "Gameplay/AuraObjectiveTypes.h"

namespace AuraObjectiveTypesPrivate
{
	constexpr float RelayDistanceUnits = 200.0f;
	constexpr double RelayDurationSeconds = 3.0;

	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	bool IsValidDefinition(const FAuraObjectiveDefinition& Definition, FString& OutError)
	{
		if (Definition.Id.IsNone() || Definition.TargetId.IsNone() || Definition.TargetCount <= 0
			|| !FMath::IsFinite(Definition.DeadlineSeconds) || Definition.DeadlineSeconds <= 0.0)
		{
			Reject(OutError, TEXT("ObjectiveDefinitionInvalid"));
			return false;
		}
		if (Definition.Type == EAuraObjectiveType::Escort
			&& (Definition.RequiredMemberIds.Num() == 0 || Definition.TargetCount != Definition.RequiredMemberIds.Num()))
		{
			Reject(OutError, TEXT("EscortDefinitionInvalid"));
			return false;
		}
		TSet<FName> Members;
		for (const FName MemberId : Definition.RequiredMemberIds)
		{
			if (MemberId.IsNone() || Members.Contains(MemberId))
			{
				Reject(OutError, TEXT("ObjectiveMemberIdentityInvalid"));
				return false;
			}
			Members.Add(MemberId);
		}
		return true;
	}

	FString MakeEventKey(const FGuid& RunId, int32 Epoch, FName ObjectiveId, int32 Generation, int32 Sequence,
		FName SubjectId)
	{
		return FString::Printf(TEXT("%s|%d|%s|%d|%d|%s"), *RunId.ToString(), Epoch, *ObjectiveId.ToString(),
			Generation, Sequence, *SubjectId.ToString());
	}
}

bool FAuraObjectiveRunState::Initialize(const FGuid& InRunId, int32 InEpoch)
{
	if (!InRunId.IsValid() || InEpoch <= 0) return false;
	*this = FAuraObjectiveRunState();
	RunId = InRunId;
	Epoch = InEpoch;
	return true;
}

bool FAuraObjectiveRunState::Touch(FString& OutError)
{
	if (Revision == MAX_int32)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveRevisionOverflow"));
		return false;
	}
	++Revision;
	return true;
}

bool FAuraObjectiveRunState::ValidateEventIdentity(const FGuid& EventRunId, int32 EventEpoch, FString& OutError) const
{
	if (!RunId.IsValid() || Epoch <= 0 || EventRunId != RunId || EventEpoch != Epoch)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ForeignObjectiveEvent"));
		return false;
	}
	return true;
}

int32 FAuraObjectiveRunState::FindObjectiveIndex(FName ObjectiveId) const
{
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (Objectives[Index].Definition.Id == ObjectiveId) return Index;
	}
	return INDEX_NONE;
}

bool FAuraObjectiveRunState::IsCurrentObjective(int32 Index, EAuraObjectiveType Type, FString& OutError) const
{
	if (!Objectives.IsValidIndex(Index) || CurrentObjectiveIndex != Index
		|| Objectives[Index].State != EAuraObjectiveState::Active || Objectives[Index].Definition.Type != Type)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveNotActive"));
		return false;
	}
	return true;
}

bool FAuraObjectiveRunState::ConfigureObjectives(const TArray<FAuraObjectiveDefinition>& Definitions, FString& OutError)
{
	OutError.Reset();
	if (!RunId.IsValid() || Epoch <= 0 || Definitions.Num() == 0)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveSequenceInvalid"));
		return false;
	}
	TMap<FName, int32> Indices;
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		if (!AuraObjectiveTypesPrivate::IsValidDefinition(Definitions[Index], OutError)
			|| Indices.Contains(Definitions[Index].Id))
		{
			if (OutError.IsEmpty()) AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveDuplicateId"));
			return false;
		}
		Indices.Add(Definitions[Index].Id, Index);
	}

	TArray<TArray<int32>> PrerequisiteIndices;
	PrerequisiteIndices.SetNum(Definitions.Num());
	TArray<int32> Colors;
	Colors.Init(0, Definitions.Num());
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		TSet<FName> SeenPrerequisites;
		for (const FName PrerequisiteId : Definitions[Index].PrerequisiteIds)
		{
			if (PrerequisiteId.IsNone() || PrerequisiteId == Definitions[Index].Id || SeenPrerequisites.Contains(PrerequisiteId))
			{
				AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectivePrerequisiteInvalid"));
				return false;
			}
			const int32* PrerequisiteIndex = Indices.Find(PrerequisiteId);
			if (!PrerequisiteIndex)
			{
				AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectivePrerequisiteMissing"));
				return false;
			}
			SeenPrerequisites.Add(PrerequisiteId);
			PrerequisiteIndices[Index].Add(*PrerequisiteIndex);
		}
	}

	TFunction<bool(int32)> Visit = [&](int32 Index)
	{
		if (Colors[Index] == 1) return false;
		if (Colors[Index] == 2) return true;
		Colors[Index] = 1;
		for (const int32 PrerequisiteIndex : PrerequisiteIndices[Index])
		{
			if (!Visit(PrerequisiteIndex)) return false;
		}
		Colors[Index] = 2;
		return true;
	};
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		if (!Visit(Index))
		{
			AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveCycleDetected"));
			return false;
		}
	}

	TArray<TArray<int32>> Dependents;
	Dependents.SetNum(Definitions.Num());
	TArray<int32> Roots;
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		if (PrerequisiteIndices[Index].Num() == 0) Roots.Add(Index);
		for (const int32 PrerequisiteIndex : PrerequisiteIndices[Index]) Dependents[PrerequisiteIndex].Add(Index);
	}
	TSet<int32> Reachable;
	TArray<int32> PendingRoots = Roots;
	while (PendingRoots.Num() > 0)
	{
		const int32 Index = PendingRoots.Pop();
		if (Reachable.Contains(Index)) continue;
		Reachable.Add(Index);
		for (const int32 Dependent : Dependents[Index]) PendingRoots.Add(Dependent);
	}
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		if (Definitions[Index].bRequired && !Reachable.Contains(Index))
		{
			AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RequiredObjectiveUnreachable"));
			return false;
		}
	}

	Objectives.Reset();
	for (const FAuraObjectiveDefinition& Definition : Definitions)
	{
		FAuraObjectiveState& State = Objectives.AddDefaulted_GetRef();
		State.Definition = Definition;
		State.TargetCount = Definition.Type == EAuraObjectiveType::Escort
			? Definition.RequiredMemberIds.Num() : Definition.TargetCount;
	}
	CurrentObjectiveIndex = INDEX_NONE;
	bExtractionOpen = false;
	Revision = 0;
	return Touch(OutError);
}

bool FAuraObjectiveRunState::TryActivateNext(double Now, FString& OutError)
{
	OutError.Reset();
	if (!FMath::IsFinite(Now) || bExtractionOpen)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveActivationRejected"));
		return false;
	}
	if (Objectives.IsValidIndex(CurrentObjectiveIndex) && Objectives[CurrentObjectiveIndex].State == EAuraObjectiveState::Active)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveAlreadyActive"));
		return false;
	}
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		FAuraObjectiveState& Candidate = Objectives[Index];
		if (Candidate.State != EAuraObjectiveState::Pending) continue;
		bool bPrerequisitesComplete = true;
		for (const FName PrerequisiteId : Candidate.Definition.PrerequisiteIds)
		{
			const int32 PrerequisiteIndex = FindObjectiveIndex(PrerequisiteId);
			if (!Objectives.IsValidIndex(PrerequisiteIndex)
				|| Objectives[PrerequisiteIndex].State != EAuraObjectiveState::Completed)
			{
				bPrerequisitesComplete = false;
				break;
			}
		}
		if (!bPrerequisitesComplete) continue;
		CurrentObjectiveIndex = Index;
		Candidate.State = EAuraObjectiveState::Active;
		Candidate.Revision = Revision + 1;
		Candidate.StartServerTime = Now;
		Candidate.DeadlineServerTime = Now + Candidate.Definition.DeadlineSeconds;
		Candidate.ProgressCount = 0;
		Candidate.LastFailureReason = NAME_None;
		Candidate.ActiveChannelOwner = NAME_None;
		Candidate.ActiveChannelTargetRevision = 0;
		Candidate.ActiveChannelEndServerTime = 0.0;
		Candidate.AcceptedEventKeys.Reset();
		Candidate.AcceptedEscortMemberIds.Reset();
		return Touch(OutError);
	}
	AuraObjectiveTypesPrivate::Reject(OutError, TEXT("NoObjectiveReady"));
	return false;
}

bool FAuraObjectiveRunState::CompleteCurrentObjective(FString& OutError)
{
	if (!Objectives.IsValidIndex(CurrentObjectiveIndex))
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveCompletionRejected"));
		return false;
	}
	Objectives[CurrentObjectiveIndex].State = EAuraObjectiveState::Completed;
	Objectives[CurrentObjectiveIndex].Revision = Revision + 1;
	Objectives[CurrentObjectiveIndex].ActiveChannelOwner = NAME_None;
	Objectives[CurrentObjectiveIndex].ActiveChannelTargetRevision = 0;
	Objectives[CurrentObjectiveIndex].ActiveChannelEndServerTime = 0.0;
	CurrentObjectiveIndex = INDEX_NONE;
	return Touch(OutError);
}

bool FAuraObjectiveRunState::FailCurrentObjective(FName Reason, FString& OutError)
{
	if (!Objectives.IsValidIndex(CurrentObjectiveIndex) || Reason.IsNone())
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("ObjectiveFailureRejected"));
		return false;
	}
	Objectives[CurrentObjectiveIndex].State = EAuraObjectiveState::Failed;
	Objectives[CurrentObjectiveIndex].LastFailureReason = Reason;
	Objectives[CurrentObjectiveIndex].Revision = Revision + 1;
	CurrentObjectiveIndex = INDEX_NONE;
	return Touch(OutError);
}

EAuraObjectiveMutationResult FAuraObjectiveRunState::RegisterClearDeath(const FGuid& EventRunId, int32 EventEpoch,
	FName ObjectiveId, int32 Generation, int32 DeathSequence, FString& OutError)
{
	OutError.Reset();
	if (!ValidateEventIdentity(EventRunId, EventEpoch, OutError) || ObjectiveId.IsNone() || Generation <= 0 || DeathSequence <= 0)
		return EAuraObjectiveMutationResult::Rejected;
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (!IsCurrentObjective(Index, EAuraObjectiveType::Clear, OutError)) return EAuraObjectiveMutationResult::Rejected;
	FAuraObjectiveState& Objective = Objectives[Index];
	if (Generation != Objective.ActiveGeneration)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("StaleObjectiveGeneration"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	const FString Key = AuraObjectiveTypesPrivate::MakeEventKey(EventRunId, EventEpoch, ObjectiveId, Generation,
		DeathSequence, NAME_None);
	if (Objective.HasAcceptedEvent(Key))
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("DuplicateObjectiveEvent"));
		return EAuraObjectiveMutationResult::Duplicate;
	}
	Objective.AcceptedEventKeys.Add(Key);
	++Objective.ProgressCount;
	if (Objective.ProgressCount >= Objective.TargetCount)
	{
		if (!CompleteCurrentObjective(OutError)) return EAuraObjectiveMutationResult::Rejected;
		return EAuraObjectiveMutationResult::Completed;
	}
	if (!Touch(OutError)) return EAuraObjectiveMutationResult::Rejected;
	return EAuraObjectiveMutationResult::Accepted;
}

EAuraObjectiveMutationResult FAuraObjectiveRunState::RegisterEscortArrival(const FGuid& EventRunId, int32 EventEpoch,
	FName ObjectiveId, FName MemberId, FName DestinationId, int32 Generation, FString& OutError)
{
	OutError.Reset();
	if (!ValidateEventIdentity(EventRunId, EventEpoch, OutError) || ObjectiveId.IsNone() || MemberId.IsNone()
		|| DestinationId.IsNone() || Generation <= 0)
		return EAuraObjectiveMutationResult::Rejected;
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (!IsCurrentObjective(Index, EAuraObjectiveType::Escort, OutError)) return EAuraObjectiveMutationResult::Rejected;
	FAuraObjectiveState& Objective = Objectives[Index];
	if (Generation != Objective.ActiveGeneration || DestinationId != Objective.Definition.TargetId
		|| !Objective.Definition.RequiredMemberIds.Contains(MemberId))
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("EscortArrivalRejected"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (Objective.AcceptedEscortMemberIds.Contains(MemberId))
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("DuplicateEscortArrival"));
		return EAuraObjectiveMutationResult::Duplicate;
	}
	const FString Key = AuraObjectiveTypesPrivate::MakeEventKey(EventRunId, EventEpoch, ObjectiveId, Generation,
		Objective.ProgressCount + 1, MemberId);
	if (Objective.HasAcceptedEvent(Key) || Objective.ProgressCount >= Objective.TargetCount)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("DuplicateEscortArrival"));
		return EAuraObjectiveMutationResult::Duplicate;
	}
	Objective.AcceptedEventKeys.Add(Key);
	Objective.AcceptedEscortMemberIds.Add(MemberId);
	++Objective.ProgressCount;
	if (Objective.ProgressCount >= Objective.TargetCount)
	{
		if (!CompleteCurrentObjective(OutError)) return EAuraObjectiveMutationResult::Rejected;
		return EAuraObjectiveMutationResult::Completed;
	}
	if (!Touch(OutError)) return EAuraObjectiveMutationResult::Rejected;
	return EAuraObjectiveMutationResult::Accepted;
}

EAuraObjectiveMutationResult FAuraObjectiveRunState::TryBeginInteractHold(const FGuid& EventRunId, int32 EventEpoch,
	FName ObjectiveId, FName OwnerId, int32 TargetRevision, double Now, bool bAlive, float DistanceUnits,
	bool bLineOfSight, FString& OutError)
{
	OutError.Reset();
	if (!ValidateEventIdentity(EventRunId, EventEpoch, OutError) || OwnerId.IsNone() || TargetRevision <= 0
		|| !FMath::IsFinite(Now) || !FMath::IsFinite(DistanceUnits))
		return EAuraObjectiveMutationResult::Rejected;
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (!IsCurrentObjective(Index, EAuraObjectiveType::InteractHold, OutError)) return EAuraObjectiveMutationResult::Rejected;
	FAuraObjectiveState& Objective = Objectives[Index];
	if (!bAlive || !bLineOfSight || DistanceUnits > AuraObjectiveTypesPrivate::RelayDistanceUnits)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayAdmissionRejected"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (Now > Objective.DeadlineServerTime + KINDA_SMALL_NUMBER)
	{
		FailCurrentObjective(TEXT("ObjectiveDeadlineExpired"), OutError);
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (!Objective.ActiveChannelOwner.IsNone())
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayAlreadyActive"));
		return EAuraObjectiveMutationResult::Busy;
	}
	Objective.ActiveChannelOwner = OwnerId;
	Objective.ActiveChannelTargetRevision = TargetRevision;
	Objective.ActiveChannelEndServerTime = Now + AuraObjectiveTypesPrivate::RelayDurationSeconds;
	Objective.StartServerTime = Now;
	if (!Touch(OutError)) return EAuraObjectiveMutationResult::Rejected;
	return EAuraObjectiveMutationResult::Accepted;
}

EAuraObjectiveMutationResult FAuraObjectiveRunState::TryCompleteInteractHold(const FGuid& EventRunId, int32 EventEpoch,
	FName ObjectiveId, FName OwnerId, int32 TargetRevision, double Now, bool bAlive, float DistanceUnits,
	bool bLineOfSight, FString& OutError)
{
	OutError.Reset();
	if (!ValidateEventIdentity(EventRunId, EventEpoch, OutError) || OwnerId.IsNone() || TargetRevision <= 0
		|| !FMath::IsFinite(Now) || !FMath::IsFinite(DistanceUnits))
		return EAuraObjectiveMutationResult::Rejected;
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (!IsCurrentObjective(Index, EAuraObjectiveType::InteractHold, OutError)) return EAuraObjectiveMutationResult::Rejected;
	FAuraObjectiveState& Objective = Objectives[Index];
	if (Objective.ActiveChannelOwner.IsNone())
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayNotActive"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (Objective.ActiveChannelOwner != OwnerId || Objective.ActiveChannelTargetRevision != TargetRevision)
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayIdentityStale"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (!bAlive || !bLineOfSight || DistanceUnits > AuraObjectiveTypesPrivate::RelayDistanceUnits)
	{
		CancelInteractHold(ObjectiveId, OwnerId, TEXT("RelayValidationLost"), OutError);
		return EAuraObjectiveMutationResult::Cancelled;
	}
	if (Now > Objective.DeadlineServerTime + KINDA_SMALL_NUMBER)
	{
		FailCurrentObjective(TEXT("ObjectiveDeadlineExpired"), OutError);
		return EAuraObjectiveMutationResult::Rejected;
	}
	if (Now > Objective.ActiveChannelEndServerTime + KINDA_SMALL_NUMBER)
	{
		CancelInteractHold(ObjectiveId, OwnerId, TEXT("RelayChannelExpired"), OutError);
		return EAuraObjectiveMutationResult::Cancelled;
	}
	Objective.ProgressCount = Objective.TargetCount;
	if (!CompleteCurrentObjective(OutError)) return EAuraObjectiveMutationResult::Rejected;
	return EAuraObjectiveMutationResult::Completed;
}

EAuraObjectiveMutationResult FAuraObjectiveRunState::CancelInteractHold(FName ObjectiveId, FName OwnerId, FName Reason,
	FString& OutError)
{
	OutError.Reset();
	if (OwnerId.IsNone() || Reason.IsNone())
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayCancellationInvalid"));
		return EAuraObjectiveMutationResult::Rejected;
	}
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (!IsCurrentObjective(Index, EAuraObjectiveType::InteractHold, OutError)) return EAuraObjectiveMutationResult::Rejected;
	FAuraObjectiveState& Objective = Objectives[Index];
	if (Objective.ActiveChannelOwner == OwnerId)
	{
		Objective.ActiveChannelOwner = NAME_None;
		Objective.ActiveChannelTargetRevision = 0;
		Objective.ActiveChannelEndServerTime = 0.0;
		Objective.LastFailureReason = Reason;
		return Touch(OutError) ? EAuraObjectiveMutationResult::Cancelled : EAuraObjectiveMutationResult::Rejected;
	}
	AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RelayNotActive"));
	return EAuraObjectiveMutationResult::Rejected;
}

bool FAuraObjectiveRunState::AreRequiredObjectivesComplete() const
{
	for (const FAuraObjectiveState& Objective : Objectives)
	{
		if (Objective.Definition.bRequired && Objective.State != EAuraObjectiveState::Completed) return false;
	}
	return Objectives.Num() > 0;
}

bool FAuraObjectiveRunState::TryBeginExtraction(FString& OutError)
{
	OutError.Reset();
	if (bExtractionOpen || !AreRequiredObjectivesComplete())
	{
		AuraObjectiveTypesPrivate::Reject(OutError, TEXT("RequiredObjectivesIncomplete"));
		return false;
	}
	bExtractionOpen = true;
	return Touch(OutError);
}

FAuraObjectivePublicSnapshot FAuraObjectiveRunState::BuildPublicSnapshot() const
{
	FAuraObjectivePublicSnapshot Snapshot;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.Revision = Revision;
	Snapshot.bExtractionOpen = bExtractionOpen;
	for (const FAuraObjectiveState& Objective : Objectives)
	{
		if (Objective.Definition.bRequired)
		{
			++Snapshot.RequiredObjectiveCount;
			if (Objective.State == EAuraObjectiveState::Completed) ++Snapshot.CompletedRequiredCount;
		}
	}
	Snapshot.bAllRequiredComplete = Snapshot.RequiredObjectiveCount > 0
		&& Snapshot.CompletedRequiredCount == Snapshot.RequiredObjectiveCount;
	if (Objectives.IsValidIndex(CurrentObjectiveIndex))
	{
		const FAuraObjectiveState& Current = Objectives[CurrentObjectiveIndex];
		Snapshot.CurrentObjectiveId = Current.Definition.Id;
		Snapshot.CurrentObjectiveState = Current.State;
		Snapshot.CurrentProgressCount = Current.ProgressCount;
		Snapshot.CurrentTargetCount = Current.TargetCount;
		Snapshot.CurrentDeadlineServerTime = Current.DeadlineServerTime;
		Snapshot.LastFailureReason = Current.LastFailureReason;
	}
	return Snapshot;
}
