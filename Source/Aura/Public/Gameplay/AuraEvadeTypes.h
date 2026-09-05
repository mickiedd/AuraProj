// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraEvadeTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraEvadeResult : uint8
{
	RejectedNotAuthority,
	RejectedInvalidOwner,
	RejectedInvalidState,
	RejectedStaleGeneration,
	RejectedCooldown,
	RejectedNoCharge,
	RejectedInvalidDirection,
	RejectedDuplicate,
	Accepted
};

UENUM(BlueprintType)
enum class EAuraEvadeSweepResult : uint8
{
	RejectedNotAuthority,
	RejectedUnknownRequest,
	RejectedOutsideMovementWindow,
	RejectedInvalidTravel,
	RejectedDuplicate,
	AcceptedFullDistance,
	AcceptedBlocked
};

/** Request identity; the server derives all timing, distance and collision facts. */
USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeRequestKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FName OwnerId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 OwnerLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 RequestSequence = 0;

	bool IsValid() const
	{
		return RunId.IsValid() && Epoch > 0 && !OwnerId.IsNone() && OwnerLifeGeneration > 0 && RequestSequence > 0;
	}

	bool operator==(const FAuraEvadeRequestKey& Other) const
	{
		return RunId == Other.RunId && Epoch == Other.Epoch && OwnerId == Other.OwnerId
			&& OwnerLifeGeneration == Other.OwnerLifeGeneration && RequestSequence == Other.RequestSequence;
	}

	bool operator!=(const FAuraEvadeRequestKey& Other) const { return !(*this == Other); }
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FAuraEvadeRequestKey Key;

	/** Client-provided normalized horizontal direction; destination and elapsed time are never accepted. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FVector Direction = FVector::ForwardVector;
};

/** Authority-observed state passed to the policy; it is not client-owned input. */
USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeOwnerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bAlive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bRunEligible = false;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeCancellationPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bCancelOwnedInteraction = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bCancelReload = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bCancelOwnedChannel = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bGrantsInvulnerability = false;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeAcceptance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	EAuraEvadeResult Result = EAuraEvadeResult::RejectedInvalidState;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FAuraEvadeRequestKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double AcceptedAtServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double MovementEndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double CooldownReadyServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	float MaxDistanceUnits = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bConsumesCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FAuraEvadeCancellationPlan CancellationPlan;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEvadeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FName OwnerId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 OwnerLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 MaxCharges = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 ChargesRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double CooldownReadyServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double AcceptedAtServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	double ActiveUntilServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bInProgress = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bHasAcceptedRequest = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	FAuraEvadeRequestKey LastAcceptedRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	int32 AcceptedRequestCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bMovementResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	bool bLastMovementBlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Evade")
	float LastTravelDistanceUnits = 0.0f;

	bool Initialize(const FGuid& InRunId, int32 InEpoch, FName InOwnerId, int32 InOwnerLifeGeneration,
		int32 InMaxCharges = 1);
	void Reset();
};
