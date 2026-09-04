// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

enum class EAuraSupplyRiskResult : uint8
{
	Rejected,
	NoChange,
	AlreadyFull,
	NotApplicable,
	Consumed,
	Claimed,
	DuplicateAccepted,
	Cancelled,
	Declined,
	ChallengeStarted,
	ChallengeCompleted,
	HitAccepted
};

struct FAuraRunSupplyMember
{
	FName MemberId = NAME_None;
	FName RoleId = NAME_None;
	bool bAlive = true;
	int32 Medkits = 2;
	int32 ResourcePacks = 1;
	float HealthNormalized = 1.0f;
	float ManaNormalized = 1.0f;
	int32 ReserveRounds = 48;
	double EmergencyCooldownEndServerTime = 0.0;
	bool bStationClaimed = false;
	TSet<int32> AcceptedRequestIds;
};

/** Inert Day 52 run-only supply, cache, and hazard contract. */
class AURA_API FAuraSupplyRiskState final
{
public:
	bool Initialize(bool bAuthority, const FGuid& InRunId, int32 InEpoch,
		const TArray<FAuraRunSupplyMember>& InMembers, FString& OutError);
	EAuraSupplyRiskResult UseMedkit(bool bAuthority, FName MemberId, int32 RequestId,
		bool bFieldMedic, FString& OutError);
	EAuraSupplyRiskResult UseResourcePack(bool bAuthority, FName MemberId, int32 RequestId,
		FString& OutError);
	EAuraSupplyRiskResult ClaimStation(bool bAuthority, FName MemberId, int32 RequestId,
		bool bChooseMedkit, FString& OutError);
	EAuraSupplyRiskResult UseEmergencyTerminal(bool bAuthority, FName MemberId, FName PadId,
		int32 RequestId, double AuthorityNow, bool bChannelCompleted, FString& OutError);
	EAuraSupplyRiskResult SubmitCacheConsent(bool bAuthority, FName MemberId, bool bAccept,
		double AuthorityNow, FString& OutError);
	EAuraSupplyRiskResult ResolveCacheTimeoutOrChallenge(bool bAuthority, double AuthorityNow,
		int32 RequestedExtraHostiles, int32 ExistingLiveHostiles, int32 ActiveHostileCap,
		bool bChallengeSucceeded, FString& OutError);
	EAuraSupplyRiskResult TryHazardHit(bool bAuthority, bool bRegisteredIdentity,
		bool bEnemyFaction, bool bCanAttack, bool bAlive, bool bValidSourceAscAndAvatar,
		bool bCurrentRunActivation, bool bEnabled, FName TargetId, bool bEligiblePlayer,
		bool bSafeZone, bool bProtectedCivilian, bool bFriendlyEnemy, int32 Activation,
		FString& OutError);
	void BeginHazardActivation(int32 Activation);
	const TArray<FAuraRunSupplyMember>& GetMembers() const { return Members; }
	int32 GetRevision() const { return Revision; }
	int32 GetHazardDamageCount() const { return HazardDamageCount; }
	int32 GetRewardCreditCount() const { return RewardCreditCount; }
	bool IsPrimaryPathOpen() const { return bPrimaryPathOpen; }
	bool IsCacheRewardEarned() const { return bCacheRewardEarned; }
	int32 GetCacheAdmittedHostiles() const { return CacheAdmittedHostiles; }

private:
	int32 FindMember(FName MemberId) const;
	bool Touch(FString& OutError);
	FGuid RunId;
	int32 Epoch = 0;
	int32 Revision = 0;
	bool bAuthority = false;
	TArray<FAuraRunSupplyMember> Members;
	TMap<FName, bool> CacheConsents;
	double CacheDecisionDeadline = 10.0;
	bool bCacheChallengeActive = false;
	bool bPrimaryPathOpen = true;
	bool bCacheRewardEarned = false;
	int32 CacheAdmittedHostiles = 0;
	int32 HazardActivation = 0;
	TSet<FName> HazardHitMembers;
	int32 HazardDamageCount = 0;
	int32 RewardCreditCount = 0;
};
