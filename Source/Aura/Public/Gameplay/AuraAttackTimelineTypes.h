// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraAttackTimelineTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraAttackTimelinePhase : uint8
{
	Idle,
	Windup,
	Committed,
	Recovery,
	Cancelled
};

UENUM(BlueprintType)
enum class EAuraAttackTimelineResult : uint8
{
	Rejected,
	Started,
	Committed,
	ImpactResolved,
	RecoveryFinished,
	Cancelled
};

/** Identity captured by the authority when an attack is admitted. */
USTRUCT(BlueprintType)
struct AURA_API FAuraAttackTimelineKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	int32 Epoch = 0;

	/** Encounter/cell generation. It changes when the logical encounter is reset or reused. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	int32 EncounterGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName EncounterId = NAME_None;

	/** Immutable behavior driver for this encounter generation. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName DriverOwner = NAME_None;

	/** Stable logical entity identity inside the encounter generation. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName SourceEntityId = NAME_None;

	/** Changes when a logical enemy actor is replaced or respawned. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	int32 SourceLifeGeneration = 0;

	/** Authority-issued sequence; clients never author this value. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	int32 AttackSequence = 0;

	bool IsValid() const
	{
		return RunId.IsValid() && Epoch > 0 && EncounterGeneration > 0 && !EncounterId.IsNone()
			&& !DriverOwner.IsNone() && !SourceEntityId.IsNone() && SourceLifeGeneration > 0 && AttackSequence > 0;
	}

	bool operator==(const FAuraAttackTimelineKey& Other) const
	{
		return RunId == Other.RunId && Epoch == Other.Epoch && EncounterGeneration == Other.EncounterGeneration
			&& EncounterId == Other.EncounterId && DriverOwner == Other.DriverOwner
			&& SourceEntityId == Other.SourceEntityId && SourceLifeGeneration == Other.SourceLifeGeneration
			&& AttackSequence == Other.AttackSequence;
	}

	bool operator!=(const FAuraAttackTimelineKey& Other) const { return !(*this == Other); }
};

/** Data selected by the authority; no client timestamp is accepted here. */
USTRUCT(BlueprintType)
struct AURA_API FAuraAttackTimelineDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName AttackDefinitionId = NAME_None;

	/** Opaque frozen damage/collision shape resolved by the archetype definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName AttackShapeId = NAME_None;

	/** Opaque presentation profile used by the cue renderer. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName TelegraphProfileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	float WindupSeconds = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	float DamageWindowSeconds = 0.05f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	float RecoverySeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	bool bShapeFrozenDuringWindup = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	bool bUsesServerFallbackTiming = true;
};

/** Authority-owned timeline facts. This struct never applies damage by itself. */
USTRUCT(BlueprintType)
struct AURA_API FAuraAttackTimelineState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FAuraAttackTimelineKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName AttackDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName AttackShapeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName TelegraphProfileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	EAuraAttackTimelinePhase Phase = EAuraAttackTimelinePhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	double AuthorityStartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	double TelegraphStartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	double ExpectedImpactServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	double DamageWindowEndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	double RecoveryEndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	FName TerminalReason = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Attack")
	bool bImpactResolved = false;

	EAuraAttackTimelineResult TryBeginWindup(const FAuraAttackTimelineKey& InKey,
		const FAuraAttackTimelineDefinition& Definition, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryCommit(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryResolveImpact(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryFinishRecovery(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryCancel(const FAuraAttackTimelineKey& CallbackKey, FName Reason, FString& OutError);

	/** Clears only terminal Cancelled state; active or normally recovering timelines cannot be reset through this path. */
	void Reset();
};
