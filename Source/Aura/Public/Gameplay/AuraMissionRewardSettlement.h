// Copyright Druid Mechanics

#pragma once
#include "CoreMinimal.h"

enum class EAuraSettlementResult : uint8 { Rejected, Committed, DuplicateAccepted };

struct FAuraSettlementMember
{
	FName MemberId = NAME_None;
	FName RoleId = NAME_None;
	int32 ProfileGeneration = 1;
	int32 WalletGold = 0;
	int32 WalletCapacity = 1000;
	bool bBoundToRun = true;
	bool bReadyAtStart = true;
	double ConnectedSeconds = 60.0;
	bool bAcceptedAction = true;
	bool bPrimaryComplete = false;
	bool bBossExtractionComplete = false;
	bool bCacheComplete = false;
	FName EarnedMasteryBadgeId = NAME_None;
	bool bMarkerCleared = false;
	uint32 GameplayGuidanceMask = 0;
	uint32 PendingGuidanceMask = 0;
	uint32 LegacyTutorialMask = 0;
	FString DurableInventoryFingerprint;
	int32 TransientSupplyCount = 0;
	int32 TransientNormalizedAmmo = 0;
	TArray<FName> TransientAugments;
	TArray<FName> MasteryBadges;
};

/** Inert Day 53 all-or-nothing settlement transaction. */
class AURA_API FAuraMissionRewardSettlement final
{
public:
	bool Initialize(bool bAuthority, const FGuid& InWorldId, const FGuid& InRunId,
		int32 InRewardVersion, const TArray<FAuraSettlementMember>& InMembers, FString& OutError);
	EAuraSettlementResult Commit(bool bAuthority, bool bTechnicalAbort, int32 InjectFailureBeforeMember,
		bool bInjectBeforeWorldReceipt, bool bInjectBeforeManifestFlip, FString& OutError);
	bool ExpireMemberToNewHubGeneration(bool bAuthority, FName MemberId, int32 NewGeneration,
		int32 NewWalletGold, FString& OutError);
	static bool HasMaximumRewardCapacity(int32 WalletGold, int32 WalletCapacity);
	static bool MigrateLegacyProfile(int32 SchemaVersion, int32& WalletGold, FString& InventoryFingerprint,
		uint32& LegacyTutorialMask, uint32& GameplayGuidanceMask, FString& OutError);
	const TArray<FAuraSettlementMember>& GetMembers() const { return Members; }
	bool IsCommitted() const { return bCommitted; }
	int32 GetWorldReceiptCount() const { return WorldReceiptCount; }
	int32 GetManifestGeneration() const { return ManifestGeneration; }

private:
	int32 FindMember(FName MemberId) const;
	bool bAuthority = false;
	bool bCommitted = false;
	FGuid WorldId;
	FGuid RunId;
	int32 RewardVersion = 0;
	int32 WorldReceiptCount = 0;
	int32 ManifestGeneration = 0;
	TArray<FAuraSettlementMember> Members;
};
