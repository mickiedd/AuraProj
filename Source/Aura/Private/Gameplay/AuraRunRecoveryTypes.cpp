// Copyright Druid Mechanics

#include "Gameplay/AuraRunRecoveryTypes.h"

namespace AuraRunRecoveryPrivate
{
	constexpr double ChannelSeconds = 3.0;
	constexpr double DisconnectLeaseSeconds = 60.0;

	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	bool SameMember(const FAuraRunRecoveryMember& A, const FAuraRunRecoveryMember& B)
	{
		return A.MemberId == B.MemberId && A.RoleId == B.RoleId && A.LifeGeneration == B.LifeGeneration
			&& A.State == B.State && A.bConnected == B.bConnected
			&& A.bTargetableDormantProxy == B.bTargetableDormantProxy
			&& FMath::IsNearlyEqual(A.DisconnectLeaseExpiryServerTime, B.DisconnectLeaseExpiryServerTime)
			&& A.DeathSequence == B.DeathSequence && A.BeaconId == B.BeaconId
			&& A.ChannelOwnerId == B.ChannelOwnerId
			&& FMath::IsNearlyEqual(A.ChannelStartServerTime, B.ChannelStartServerTime)
			&& FMath::IsNearlyEqual(A.HealthNormalized, B.HealthNormalized)
			&& FMath::IsNearlyEqual(A.ManaNormalized, B.ManaNormalized)
			&& A.RetainedState == B.RetainedState;
	}
}

bool FAuraRunRecoveryRetainedState::operator==(const FAuraRunRecoveryRetainedState& Other) const
{
	return MagazineRounds == Other.MagazineRounds && ReserveRounds == Other.ReserveRounds
		&& AmmoRevision == Other.AmmoRevision && SupplyCount == Other.SupplyCount
		&& AugmentIds == Other.AugmentIds
		&& FMath::IsNearlyEqual(AbilityCooldownEndServerTime, Other.AbilityCooldownEndServerTime)
		&& FMath::IsNearlyEqual(EvadeCooldownEndServerTime, Other.EvadeCooldownEndServerTime)
		&& FMath::IsNearlyEqual(SupplyCooldownEndServerTime, Other.SupplyCooldownEndServerTime);
}

bool FAuraRunRecoverySnapshot::operator==(const FAuraRunRecoverySnapshot& Other) const
{
	if (RunId != Other.RunId || Epoch != Other.Epoch || Revision != Other.Revision
		|| ChargesRemaining != Other.ChargesRemaining || ReservedCharges != Other.ReservedCharges
		|| AvailableCharges != Other.AvailableCharges || BeaconCount != Other.BeaconCount
		|| AlivePublicationCount != Other.AlivePublicationCount || LegacySaveCount != Other.LegacySaveCount
		|| bTerminal != Other.bTerminal || TerminalReason != Other.TerminalReason
		|| Members.Num() != Other.Members.Num() || Receipts.Num() != Other.Receipts.Num()) return false;
	for (int32 Index = 0; Index < Members.Num(); ++Index)
		if (!AuraRunRecoveryPrivate::SameMember(Members[Index], Other.Members[Index])) return false;
	for (int32 Index = 0; Index < Receipts.Num(); ++Index)
	{
		const FAuraRunRecoveryReceipt& A = Receipts[Index];
		const FAuraRunRecoveryReceipt& B = Other.Receipts[Index];
		if (A.Serial != B.Serial || A.TargetMemberId != B.TargetMemberId || A.DeathSequence != B.DeathSequence
			|| A.RetryCount != B.RetryCount
			|| !FMath::IsNearlyEqual(A.LastAttemptServerTime, B.LastAttemptServerTime)
			|| A.bCommitted != B.bCommitted || A.bTerminal != B.bTerminal) return false;
	}
	return true;
}

bool FAuraRunRecoveryState::UsesCooperativeRecovery(FName GameplayProfile)
{
	return GameplayProfile == TEXT("GameplayExpansionV1");
}

bool FAuraRunRecoveryState::Initialize(bool bInAuthority, const FGuid& InRunId, int32 InEpoch,
	FName InGameplayProfile, int32 InParticipantCount, double StartServerTime, double RunDeadlineSeconds,
	const TArray<FAuraRunRecoveryMember>& InMembers, FString& OutError)
{
	OutError.Reset();
	*this = FAuraRunRecoveryState();
	if (!bInAuthority || !InRunId.IsValid() || InEpoch <= 0 || !UsesCooperativeRecovery(InGameplayProfile)
		|| (InParticipantCount != 1 && InParticipantCount != 2) || InMembers.Num() != InParticipantCount
		|| !FMath::IsFinite(StartServerTime) || !FMath::IsFinite(RunDeadlineSeconds)
		|| RunDeadlineSeconds <= 0.0 || StartServerTime > TNumericLimits<double>::Max() - RunDeadlineSeconds)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryIdentityInvalid"));
		return false;
	}
	TSet<FName> Seen;
	for (const FAuraRunRecoveryMember& Member : InMembers)
	{
		if (Member.MemberId.IsNone() || Member.RoleId.IsNone() || Seen.Contains(Member.MemberId)
			|| Member.LifeGeneration <= 0 || Member.State != EAuraRunRecoveryMemberState::Alive
			|| !Member.bConnected || Member.bTargetableDormantProxy || Member.DeathSequence != 0
			|| !Member.BeaconId.IsNone() || !Member.ChannelOwnerId.IsNone()
			|| !FMath::IsNearlyEqual(Member.HealthNormalized, 1.0f)
			|| !FMath::IsNearlyEqual(Member.ManaNormalized, 1.0f))
		{
			AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryMemberInvalid"));
			return false;
		}
		Seen.Add(Member.MemberId);
	}
	bAuthority = true;
	RunId = InRunId;
	Epoch = InEpoch;
	ParticipantCount = InParticipantCount;
	ChargesRemaining = InParticipantCount == 1 ? 1 : 2;
	RunDeadlineServerTime = StartServerTime + RunDeadlineSeconds;
	Members = InMembers;
	Members.Sort([](const FAuraRunRecoveryMember& A, const FAuraRunRecoveryMember& B)
	{
		return A.MemberId.ToString() < B.MemberId.ToString();
	});
	return Touch(OutError);
}

bool FAuraRunRecoveryState::Touch(FString& OutError)
{
	if (Revision == MAX_int32)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryRevisionOverflow"));
		return false;
	}
	++Revision;
	return true;
}

int32 FAuraRunRecoveryState::FindMember(FName MemberId) const
{
	for (int32 Index = 0; Index < Members.Num(); ++Index)
		if (Members[Index].MemberId == MemberId) return Index;
	return INDEX_NONE;
}

int32 FAuraRunRecoveryState::FindReceipt(int32 Serial) const
{
	for (int32 Index = 0; Index < Receipts.Num(); ++Index)
		if (Receipts[Index].Serial == Serial) return Index;
	return INDEX_NONE;
}

int32 FAuraRunRecoveryState::FindLiveReceipt(FName TargetMemberId, int32 DeathSequence) const
{
	for (int32 Index = 0; Index < Receipts.Num(); ++Index)
		if (Receipts[Index].TargetMemberId == TargetMemberId && Receipts[Index].DeathSequence == DeathSequence
			&& !Receipts[Index].bTerminal) return Index;
	return INDEX_NONE;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::EvaluateWipe(FString& OutError)
{
	if (bTerminal) return EAuraRunRecoveryResult::NoChange;
	bool bRetainedAliveMember = false;
	for (const FAuraRunRecoveryMember& Member : Members)
	{
		if (Member.State == EAuraRunRecoveryMemberState::Alive
			&& (Member.bConnected || Member.bTargetableDormantProxy))
		{
			bRetainedAliveMember = true;
			break;
		}
	}
	const bool bSoloRecoveryStillAvailable = ParticipantCount == 1 && ChargesRemaining - ReservedCharges > 0;
	if (bRetainedAliveMember || bSoloRecoveryStillAvailable) return EAuraRunRecoveryResult::NoChange;
	for (FAuraRunRecoveryReceipt& Receipt : Receipts)
	{
		if (!Receipt.bTerminal) Receipt.bTerminal = true;
	}
	ReservedCharges = 0;
	for (FAuraRunRecoveryMember& Member : Members)
	{
		Member.BeaconId = NAME_None;
		Member.ChannelOwnerId = NAME_None;
		Member.ChannelStartServerTime = 0.0;
	}
	bTerminal = true;
	TerminalReason = TEXT("TeamWipe");
	return Touch(OutError) ? EAuraRunRecoveryResult::WipeFailed : EAuraRunRecoveryResult::Rejected;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::RegisterDeath(bool bInAuthority, FName MemberId,
	int32 LifeGeneration, int32 DeathSequence, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || bTerminal || !Members.IsValidIndex(Index)
		|| !FMath::IsFinite(AuthorityNow) || AuthorityNow >= RunDeadlineServerTime
		|| LifeGeneration != Members[Index].LifeGeneration || DeathSequence <= 0)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryDeathRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	FAuraRunRecoveryMember& Member = Members[Index];
	if (DeathSequence == Member.DeathSequence && Member.State != EAuraRunRecoveryMemberState::Alive)
		return EAuraRunRecoveryResult::DuplicateDeath;
	if (DeathSequence <= Member.DeathSequence || Member.State != EAuraRunRecoveryMemberState::Alive)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryDeathSequenceRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	Member.State = EAuraRunRecoveryMemberState::AwaitingRescue;
	Member.DeathSequence = DeathSequence;
	Member.HealthNormalized = 0.0f;
	Member.bTargetableDormantProxy = false;
	Member.DisconnectLeaseExpiryServerTime = 0.0;
	Member.BeaconId = FName(*FString::Printf(TEXT("recovery_%s_%d"), *MemberId.ToString(), DeathSequence));
	Member.ChannelOwnerId = ParticipantCount == 1 ? FName(TEXT("Checkpoint")) : NAME_None;
	Member.ChannelStartServerTime = ParticipantCount == 1 ? AuthorityNow : 0.0;
	if (!Touch(OutError)) return EAuraRunRecoveryResult::Rejected;
	const EAuraRunRecoveryResult Wipe = EvaluateWipe(OutError);
	return Wipe == EAuraRunRecoveryResult::WipeFailed ? Wipe : EAuraRunRecoveryResult::DeathAccepted;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::BeginRecoveryChannel(bool bInAuthority,
	FName RequesterMemberId, FName TargetMemberId, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	const int32 RequesterIndex = FindMember(RequesterMemberId);
	const int32 TargetIndex = FindMember(TargetMemberId);
	if (!bAuthority || !bInAuthority || bTerminal || !Members.IsValidIndex(RequesterIndex)
		|| !Members.IsValidIndex(TargetIndex) || RequesterIndex == TargetIndex || !FMath::IsFinite(AuthorityNow)
		|| AuthorityNow >= RunDeadlineServerTime || Members[RequesterIndex].State != EAuraRunRecoveryMemberState::Alive
		|| !Members[RequesterIndex].bConnected
		|| Members[TargetIndex].State != EAuraRunRecoveryMemberState::AwaitingRescue)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryChannelRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	Members[TargetIndex].ChannelOwnerId = RequesterMemberId;
	Members[TargetIndex].ChannelStartServerTime = AuthorityNow;
	return Touch(OutError) ? EAuraRunRecoveryResult::ChannelStarted : EAuraRunRecoveryResult::Rejected;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::TryReserveRecovery(bool bInAuthority,
	FName RequesterMemberId, FName TargetMemberId, int32 DeathSequence, double AuthorityNow,
	int32& OutReceiptSerial, FString& OutError)
{
	OutError.Reset();
	OutReceiptSerial = 0;
	const int32 TargetIndex = FindMember(TargetMemberId);
	if (!bAuthority || !bInAuthority || bTerminal || !Members.IsValidIndex(TargetIndex)
		|| !FMath::IsFinite(AuthorityNow) || AuthorityNow >= RunDeadlineServerTime)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryReservationRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	FAuraRunRecoveryMember& Target = Members[TargetIndex];
	const int32 RequesterIndex = FindMember(RequesterMemberId);
	const bool bValidRequester = ParticipantCount == 1 && RequesterMemberId == TEXT("Checkpoint")
		? TargetMemberId == Members[0].MemberId
		: Members.IsValidIndex(RequesterIndex) && RequesterIndex != TargetIndex
			&& Members[RequesterIndex].State == EAuraRunRecoveryMemberState::Alive
			&& Members[RequesterIndex].bConnected;
	const int32 ExistingReceipt = FindLiveReceipt(TargetMemberId, DeathSequence);
	if (Receipts.IsValidIndex(ExistingReceipt))
	{
		OutReceiptSerial = Receipts[ExistingReceipt].Serial;
		return EAuraRunRecoveryResult::DuplicateAccepted;
	}
	if (!bValidRequester || Target.State != EAuraRunRecoveryMemberState::AwaitingRescue || Target.DeathSequence != DeathSequence
		|| Target.ChannelOwnerId != RequesterMemberId
		|| AuthorityNow + KINDA_SMALL_NUMBER < Target.ChannelStartServerTime + AuraRunRecoveryPrivate::ChannelSeconds
		|| ChargesRemaining - ReservedCharges <= 0 || NextReceiptSerial == MAX_int32)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryReservationNotReady"));
		return EAuraRunRecoveryResult::Rejected;
	}
	FAuraRunRecoveryReceipt& Receipt = Receipts.AddDefaulted_GetRef();
	Receipt.Serial = NextReceiptSerial++;
	Receipt.TargetMemberId = TargetMemberId;
	Receipt.DeathSequence = DeathSequence;
	++ReservedCharges;
	Target.State = EAuraRunRecoveryMemberState::Recovering;
	OutReceiptSerial = Receipt.Serial;
	return Touch(OutError) ? EAuraRunRecoveryResult::ChargeReserved : EAuraRunRecoveryResult::Rejected;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::CommitRecoveryAttachment(bool bInAuthority,
	int32 ReceiptSerial, bool bPawnCreated, bool bAscAttached, bool bLoadoutAttached,
	double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	const int32 ReceiptIndex = FindReceipt(ReceiptSerial);
	if (!bAuthority || !bInAuthority || bTerminal || !Receipts.IsValidIndex(ReceiptIndex)
		|| !FMath::IsFinite(AuthorityNow) || AuthorityNow >= RunDeadlineServerTime)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryAttachmentRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	FAuraRunRecoveryReceipt& Receipt = Receipts[ReceiptIndex];
	const int32 MemberIndex = FindMember(Receipt.TargetMemberId);
	if (Receipt.bTerminal || Receipt.bCommitted || !Members.IsValidIndex(MemberIndex)
		|| Members[MemberIndex].State != EAuraRunRecoveryMemberState::Recovering
		|| Members[MemberIndex].DeathSequence != Receipt.DeathSequence)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryAttachmentIdentityRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	if (!bPawnCreated || !bAscAttached || !bLoadoutAttached)
	{
		if (Receipt.RetryCount == 0)
		{
			++Receipt.RetryCount;
			Receipt.LastAttemptServerTime = AuthorityNow;
			return Touch(OutError) ? EAuraRunRecoveryResult::AttachmentRetryScheduled
				: EAuraRunRecoveryResult::Rejected;
		}
		if (AuthorityNow + KINDA_SMALL_NUMBER < Receipt.LastAttemptServerTime + 1.0)
		{
			AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryAttachmentRetryTooEarly"));
			return EAuraRunRecoveryResult::Rejected;
		}
		Receipt.bTerminal = true;
		--ReservedCharges;
		Members[MemberIndex].State = EAuraRunRecoveryMemberState::AwaitingRescue;
		bTerminal = true;
		TerminalReason = TEXT("TechnicalRecoveryFailure");
		return Touch(OutError) ? EAuraRunRecoveryResult::TechnicalRecoveryFailure
			: EAuraRunRecoveryResult::Rejected;
	}
	Receipt.bCommitted = true;
	Receipt.bTerminal = true;
	--ReservedCharges;
	--ChargesRemaining;
	FAuraRunRecoveryMember& Member = Members[MemberIndex];
	Member.State = EAuraRunRecoveryMemberState::Alive;
	Member.HealthNormalized = 0.35f;
	Member.ManaNormalized = 0.50f;
	Member.BeaconId = NAME_None;
	Member.ChannelOwnerId = NAME_None;
	Member.ChannelStartServerTime = 0.0;
	Member.bTargetableDormantProxy = !Member.bConnected;
	if (Member.bTargetableDormantProxy)
		Member.DisconnectLeaseExpiryServerTime = AuthorityNow + AuraRunRecoveryPrivate::DisconnectLeaseSeconds;
	++AlivePublicationCount;
	return Touch(OutError) ? EAuraRunRecoveryResult::Recovered : EAuraRunRecoveryResult::Rejected;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::SetConnected(bool bInAuthority, FName MemberId,
	bool bConnected, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || bTerminal || !Members.IsValidIndex(Index)
		|| !FMath::IsFinite(AuthorityNow) || AuthorityNow >= RunDeadlineServerTime)
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryConnectionRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	FAuraRunRecoveryMember& Member = Members[Index];
	if (Member.bConnected == bConnected) return EAuraRunRecoveryResult::NoChange;
	Member.bConnected = bConnected;
	if (Member.State == EAuraRunRecoveryMemberState::Alive)
	{
		if (bConnected)
		{
			Member.bTargetableDormantProxy = false;
			Member.DisconnectLeaseExpiryServerTime = 0.0;
		}
		else
		{
			Member.bTargetableDormantProxy = true;
			Member.DisconnectLeaseExpiryServerTime = AuthorityNow + AuraRunRecoveryPrivate::DisconnectLeaseSeconds;
		}
	}
	return Touch(OutError) ? EAuraRunRecoveryResult::NoChange : EAuraRunRecoveryResult::Rejected;
}

EAuraRunRecoveryResult FAuraRunRecoveryState::Tick(bool bInAuthority, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || bTerminal || !FMath::IsFinite(AuthorityNow))
	{
		AuraRunRecoveryPrivate::Reject(OutError, TEXT("RecoveryTickRejected"));
		return EAuraRunRecoveryResult::Rejected;
	}
	if (AuthorityNow >= RunDeadlineServerTime)
	{
		bTerminal = true;
		TerminalReason = TEXT("RunDeadlineExpired");
		return Touch(OutError) ? EAuraRunRecoveryResult::TechnicalRecoveryFailure
			: EAuraRunRecoveryResult::Rejected;
	}
	bool bExpired = false;
	for (FAuraRunRecoveryMember& Member : Members)
	{
		if (Member.State == EAuraRunRecoveryMemberState::Alive && !Member.bConnected
			&& Member.bTargetableDormantProxy && AuthorityNow >= Member.DisconnectLeaseExpiryServerTime)
		{
			Member.State = EAuraRunRecoveryMemberState::Forfeited;
			Member.bTargetableDormantProxy = false;
			Member.DisconnectLeaseExpiryServerTime = 0.0;
			bExpired = true;
		}
	}
	if (!bExpired) return EAuraRunRecoveryResult::NoChange;
	if (!Touch(OutError)) return EAuraRunRecoveryResult::Rejected;
	const EAuraRunRecoveryResult Wipe = EvaluateWipe(OutError);
	return Wipe == EAuraRunRecoveryResult::WipeFailed ? Wipe : EAuraRunRecoveryResult::LeaseExpired;
}

FAuraRunRecoverySnapshot FAuraRunRecoveryState::BuildSnapshot() const
{
	FAuraRunRecoverySnapshot Snapshot;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.Revision = Revision;
	Snapshot.ChargesRemaining = ChargesRemaining;
	Snapshot.ReservedCharges = ReservedCharges;
	Snapshot.AvailableCharges = FMath::Max(0, ChargesRemaining - ReservedCharges);
	Snapshot.AlivePublicationCount = AlivePublicationCount;
	Snapshot.bTerminal = bTerminal;
	Snapshot.TerminalReason = TerminalReason;
	Snapshot.Members = Members;
	Snapshot.Receipts = Receipts;
	for (const FAuraRunRecoveryMember& Member : Members)
		if (!Member.BeaconId.IsNone()) ++Snapshot.BeaconCount;
	return Snapshot;
}
