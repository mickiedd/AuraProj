// Copyright Druid Mechanics

#include "Gameplay/AuraMissionRewardSettlement.h"

bool FAuraMissionRewardSettlement::Initialize(bool bInAuthority, const FGuid& InWorldId,
	const FGuid& InRunId, int32 InRewardVersion, const TArray<FAuraSettlementMember>& InMembers,
	FString& OutError)
{
	OutError.Reset(); *this = FAuraMissionRewardSettlement();
	if (!bInAuthority || !InWorldId.IsValid() || !InRunId.IsValid() || InRewardVersion <= 0 || InMembers.IsEmpty())
	{ OutError = TEXT("SettlementIdentityInvalid"); return false; }
	TSet<FName> Seen;
	for (const FAuraSettlementMember& Member : InMembers)
	{
		if (Member.MemberId.IsNone() || Member.RoleId.IsNone() || Seen.Contains(Member.MemberId)
			|| Member.ProfileGeneration <= 0 || Member.WalletGold < 0 || Member.WalletCapacity < Member.WalletGold)
		{ OutError = TEXT("SettlementMemberInvalid"); return false; }
		Seen.Add(Member.MemberId);
	}
	bAuthority = true; WorldId = InWorldId; RunId = InRunId; RewardVersion = InRewardVersion;
	Members = InMembers; return true;
}

int32 FAuraMissionRewardSettlement::FindMember(FName MemberId) const
{
	for (int32 Index=0; Index<Members.Num(); ++Index) if (Members[Index].MemberId==MemberId) return Index;
	return INDEX_NONE;
}

bool FAuraMissionRewardSettlement::HasMaximumRewardCapacity(int32 WalletGold, int32 WalletCapacity)
{
	return WalletGold >= 0 && WalletCapacity >= WalletGold && WalletCapacity - WalletGold >= 100;
}

EAuraSettlementResult FAuraMissionRewardSettlement::Commit(bool bInAuthority, bool bTechnicalAbort,
	int32 InjectFailureBeforeMember, bool bInjectBeforeWorldReceipt, bool bInjectBeforeManifestFlip,
	FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority) { OutError=TEXT("SettlementAuthorityRejected"); return EAuraSettlementResult::Rejected; }
	if (bCommitted) return EAuraSettlementResult::DuplicateAccepted;
	TArray<FAuraSettlementMember> Prepared = Members;
	int32 PreparedIndex = 0;
	for (FAuraSettlementMember& Member : Prepared)
	{
		if (!Member.bBoundToRun) continue;
		if (InjectFailureBeforeMember == PreparedIndex++) { OutError=TEXT("InjectedProfileWriteFailure"); return EAuraSettlementResult::Rejected; }
		const bool bEligible = Member.bReadyAtStart && Member.ConnectedSeconds >= 60.0 && Member.bAcceptedAction;
		int32 Payout = 0;
		if (!bTechnicalAbort && bEligible)
		{
			if (Member.bPrimaryComplete) Payout += 50;
			if (Member.bBossExtractionComplete) Payout += 25;
			if (Member.bCacheComplete) Payout += 25;
		}
		if (Payout < 0 || Payout > 100 || Member.WalletGold > Member.WalletCapacity - Payout)
		{ OutError=TEXT("SettlementWalletOverflow"); return EAuraSettlementResult::Rejected; }
		Member.WalletGold += Payout;
		Member.bMarkerCleared = true;
		if (!bTechnicalAbort) Member.GameplayGuidanceMask |= Member.PendingGuidanceMask;
		if (!bTechnicalAbort && bEligible && Member.bPrimaryComplete && !Member.EarnedMasteryBadgeId.IsNone())
		{
			Member.MasteryBadges.AddUnique(Member.EarnedMasteryBadgeId);
		}
	}
	if (bInjectBeforeWorldReceipt) { OutError=TEXT("InjectedWorldReceiptFailure"); return EAuraSettlementResult::Rejected; }
	if (bInjectBeforeManifestFlip) { OutError=TEXT("InjectedManifestFlipFailure"); return EAuraSettlementResult::Rejected; }
	Members = MoveTemp(Prepared); WorldReceiptCount = 1; ++ManifestGeneration; bCommitted = true;
	return EAuraSettlementResult::Committed;
}

bool FAuraMissionRewardSettlement::ExpireMemberToNewHubGeneration(bool bInAuthority, FName MemberId,
	int32 NewGeneration, int32 NewWalletGold, FString& OutError)
{
	OutError.Reset(); const int32 Index=FindMember(MemberId);
	if (!bAuthority || !bInAuthority || !Members.IsValidIndex(Index) || NewGeneration<=Members[Index].ProfileGeneration
		|| NewWalletGold<0) { OutError=TEXT("SettlementExpiryRejected"); return false; }
	Members[Index].bBoundToRun=false; Members[Index].bMarkerCleared=true;
	Members[Index].ProfileGeneration=NewGeneration; Members[Index].WalletGold=NewWalletGold;
	Members[Index].GameplayGuidanceMask |= Members[Index].PendingGuidanceMask;
	return true;
}

bool FAuraMissionRewardSettlement::MigrateLegacyProfile(int32 SchemaVersion, int32& WalletGold,
	FString& InventoryFingerprint, uint32& LegacyTutorialMask, uint32& GameplayGuidanceMask,
	FString& OutError)
{
	OutError.Reset();
	if (SchemaVersion < 1 || SchemaVersion > 2 || WalletGold < 0 || InventoryFingerprint.IsEmpty())
	{ OutError=TEXT("SaveMigrationRejected"); return false; }
	if (SchemaVersion == 1) GameplayGuidanceMask = 0;
	return true;
}
