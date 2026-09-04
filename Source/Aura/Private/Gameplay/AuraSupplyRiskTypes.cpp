// Copyright Druid Mechanics

#include "Gameplay/AuraSupplyRiskTypes.h"

namespace AuraSupplyRiskPrivate
{
	void Reject(FString& OutError, const TCHAR* Reason) { OutError = Reason; }
}

bool FAuraSupplyRiskState::Initialize(bool bInAuthority, const FGuid& InRunId, int32 InEpoch,
	const TArray<FAuraRunSupplyMember>& InMembers, FString& OutError)
{
	OutError.Reset();
	*this = FAuraSupplyRiskState();
	if (!bInAuthority || !InRunId.IsValid() || InEpoch <= 0 || InMembers.IsEmpty() || InMembers.Num() > 2)
	{
		AuraSupplyRiskPrivate::Reject(OutError, TEXT("SupplyIdentityInvalid"));
		return false;
	}
	TSet<FName> Seen;
	for (const FAuraRunSupplyMember& Member : InMembers)
	{
		if (Member.MemberId.IsNone() || Seen.Contains(Member.MemberId)
			|| (Member.RoleId != TEXT("Aura") && Member.RoleId != TEXT("BungeeMan"))
			|| Member.Medkits < 0 || Member.Medkits > 2 || Member.ResourcePacks < 0 || Member.ResourcePacks > 1
			|| !FMath::IsWithinInclusive(Member.HealthNormalized, 0.0f, 1.0f)
			|| !FMath::IsWithinInclusive(Member.ManaNormalized, 0.0f, 1.0f)
			|| Member.ReserveRounds < 0 || Member.ReserveRounds > 48)
		{
			AuraSupplyRiskPrivate::Reject(OutError, TEXT("SupplyMemberInvalid"));
			return false;
		}
		Seen.Add(Member.MemberId);
	}
	bAuthority = true;
	RunId = InRunId;
	Epoch = InEpoch;
	Members = InMembers;
	return Touch(OutError);
}

int32 FAuraSupplyRiskState::FindMember(FName MemberId) const
{
	for (int32 Index = 0; Index < Members.Num(); ++Index) if (Members[Index].MemberId == MemberId) return Index;
	return INDEX_NONE;
}

bool FAuraSupplyRiskState::Touch(FString& OutError)
{
	if (Revision == MAX_int32) { AuraSupplyRiskPrivate::Reject(OutError, TEXT("SupplyRevisionOverflow")); return false; }
	++Revision;
	return true;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::UseMedkit(bool bInAuthority, FName MemberId,
	int32 RequestId, bool bFieldMedic, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || !Members.IsValidIndex(Index) || RequestId <= 0 || !Members[Index].bAlive)
		return EAuraSupplyRiskResult::Rejected;
	FAuraRunSupplyMember& Member = Members[Index];
	if (Member.AcceptedRequestIds.Contains(RequestId)) return EAuraSupplyRiskResult::DuplicateAccepted;
	if (Member.HealthNormalized >= 1.0f - KINDA_SMALL_NUMBER) return EAuraSupplyRiskResult::AlreadyFull;
	if (Member.Medkits <= 0) return EAuraSupplyRiskResult::Rejected;
	Member.HealthNormalized = FMath::Min(1.0f, Member.HealthNormalized + (bFieldMedic ? 0.45f : 0.35f));
	--Member.Medkits;
	Member.AcceptedRequestIds.Add(RequestId);
	return Touch(OutError) ? EAuraSupplyRiskResult::Consumed : EAuraSupplyRiskResult::Rejected;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::UseResourcePack(bool bInAuthority, FName MemberId,
	int32 RequestId, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || !Members.IsValidIndex(Index) || RequestId <= 0 || !Members[Index].bAlive)
		return EAuraSupplyRiskResult::Rejected;
	FAuraRunSupplyMember& Member = Members[Index];
	if (Member.AcceptedRequestIds.Contains(RequestId)) return EAuraSupplyRiskResult::DuplicateAccepted;
	const bool bFull = Member.RoleId == TEXT("Aura") ? Member.ManaNormalized >= 1.0f - KINDA_SMALL_NUMBER
		: Member.ReserveRounds >= 48;
	if (bFull) return EAuraSupplyRiskResult::AlreadyFull;
	if (Member.ResourcePacks <= 0) return EAuraSupplyRiskResult::Rejected;
	if (Member.RoleId == TEXT("Aura")) Member.ManaNormalized = FMath::Min(1.0f, Member.ManaNormalized + 0.5f);
	else Member.ReserveRounds = FMath::Min(48, Member.ReserveRounds + 24);
	--Member.ResourcePacks;
	Member.AcceptedRequestIds.Add(RequestId);
	return Touch(OutError) ? EAuraSupplyRiskResult::Consumed : EAuraSupplyRiskResult::Rejected;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::ClaimStation(bool bInAuthority, FName MemberId,
	int32 RequestId, bool bChooseMedkit, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || !Members.IsValidIndex(Index) || RequestId <= 0 || !Members[Index].bAlive)
		return EAuraSupplyRiskResult::Rejected;
	FAuraRunSupplyMember& Member = Members[Index];
	if (Member.bStationClaimed) return EAuraSupplyRiskResult::DuplicateAccepted;
	if (bChooseMedkit)
	{
		if (Member.Medkits >= 2) return EAuraSupplyRiskResult::AlreadyFull;
		++Member.Medkits;
	}
	else if (Member.RoleId == TEXT("Aura")) Member.ManaNormalized = 1.0f;
	else Member.ReserveRounds = 48;
	Member.bStationClaimed = true;
	Member.AcceptedRequestIds.Add(RequestId);
	return Touch(OutError) ? EAuraSupplyRiskResult::Claimed : EAuraSupplyRiskResult::Rejected;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::UseEmergencyTerminal(bool bInAuthority, FName MemberId,
	FName PadId, int32 RequestId, double AuthorityNow, bool bChannelCompleted, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindMember(MemberId);
	if (!bAuthority || !bInAuthority || !Members.IsValidIndex(Index) || PadId.IsNone() || RequestId <= 0
		|| !FMath::IsFinite(AuthorityNow) || !Members[Index].bAlive) return EAuraSupplyRiskResult::Rejected;
	FAuraRunSupplyMember& Member = Members[Index];
	if (Member.AcceptedRequestIds.Contains(RequestId)) return EAuraSupplyRiskResult::DuplicateAccepted;
	if (!bChannelCompleted) return EAuraSupplyRiskResult::Cancelled;
	if (AuthorityNow + KINDA_SMALL_NUMBER < Member.EmergencyCooldownEndServerTime) return EAuraSupplyRiskResult::Rejected;
	if (Member.RoleId == TEXT("Aura"))
	{
		if (Member.ManaNormalized >= 1.0f - KINDA_SMALL_NUMBER) return EAuraSupplyRiskResult::AlreadyFull;
		Member.ManaNormalized = FMath::Min(1.0f, Member.ManaNormalized + 0.2f);
	}
	else
	{
		if (Member.ReserveRounds >= 48) return EAuraSupplyRiskResult::AlreadyFull;
		Member.ReserveRounds = FMath::Min(48, Member.ReserveRounds + 6);
	}
	Member.EmergencyCooldownEndServerTime = AuthorityNow + 30.0;
	Member.AcceptedRequestIds.Add(RequestId);
	return Touch(OutError) ? EAuraSupplyRiskResult::Consumed : EAuraSupplyRiskResult::Rejected;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::SubmitCacheConsent(bool bInAuthority, FName MemberId,
	bool bAccept, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || FindMember(MemberId) == INDEX_NONE || !FMath::IsFinite(AuthorityNow)
		|| AuthorityNow > CacheDecisionDeadline || CacheConsents.Contains(MemberId)) return EAuraSupplyRiskResult::Rejected;
	CacheConsents.Add(MemberId, bAccept);
	if (!bAccept) return Touch(OutError) ? EAuraSupplyRiskResult::Declined : EAuraSupplyRiskResult::Rejected;
	if (CacheConsents.Num() == Members.Num())
	{
		for (const TPair<FName, bool>& Consent : CacheConsents) if (!Consent.Value) return EAuraSupplyRiskResult::Declined;
		bCacheChallengeActive = true;
		Touch(OutError);
		return EAuraSupplyRiskResult::ChallengeStarted;
	}
	Touch(OutError);
	return EAuraSupplyRiskResult::NoChange;
}

EAuraSupplyRiskResult FAuraSupplyRiskState::ResolveCacheTimeoutOrChallenge(bool bInAuthority,
	double AuthorityNow, int32 RequestedExtraHostiles, int32 ExistingLiveHostiles, int32 ActiveHostileCap,
	bool bChallengeSucceeded, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !FMath::IsFinite(AuthorityNow)) return EAuraSupplyRiskResult::Rejected;
	if (!bCacheChallengeActive)
	{
		if (AuthorityNow < CacheDecisionDeadline) return EAuraSupplyRiskResult::NoChange;
		return EAuraSupplyRiskResult::Declined;
	}
	if (RequestedExtraHostiles < 0 || ExistingLiveHostiles < 0 || ActiveHostileCap <= 0
		|| RequestedExtraHostiles > 4 || ExistingLiveHostiles + RequestedExtraHostiles > ActiveHostileCap)
		return EAuraSupplyRiskResult::Rejected;
	CacheAdmittedHostiles = RequestedExtraHostiles;
	bCacheRewardEarned = bChallengeSucceeded;
	bCacheChallengeActive = false;
	return Touch(OutError) ? EAuraSupplyRiskResult::ChallengeCompleted : EAuraSupplyRiskResult::Rejected;
}

void FAuraSupplyRiskState::BeginHazardActivation(int32 Activation)
{
	if (Activation > HazardActivation) { HazardActivation = Activation; HazardHitMembers.Reset(); }
}

EAuraSupplyRiskResult FAuraSupplyRiskState::TryHazardHit(bool bInAuthority, bool bRegisteredIdentity,
	bool bEnemyFaction, bool bCanAttack, bool bAlive, bool bValidSourceAscAndAvatar,
	bool bCurrentRunActivation, bool bEnabled, FName TargetId, bool bEligiblePlayer,
	bool bSafeZone, bool bProtectedCivilian, bool bFriendlyEnemy, int32 Activation, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bRegisteredIdentity || !bEnemyFaction || !bCanAttack || !bAlive
		|| !bValidSourceAscAndAvatar || !bCurrentRunActivation || !bEnabled || TargetId.IsNone()
		|| !bEligiblePlayer || bSafeZone || bProtectedCivilian || bFriendlyEnemy
		|| Activation <= 0 || Activation != HazardActivation || HazardHitMembers.Contains(TargetId))
	{
		AuraSupplyRiskPrivate::Reject(OutError, TEXT("HazardHitRejected"));
		return EAuraSupplyRiskResult::Rejected;
	}
	HazardHitMembers.Add(TargetId);
	++HazardDamageCount;
	return Touch(OutError) ? EAuraSupplyRiskResult::HitAccepted : EAuraSupplyRiskResult::Rejected;
}
