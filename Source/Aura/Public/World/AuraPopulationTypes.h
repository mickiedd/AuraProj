// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AuraPopulationTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraPopulationSlotState : uint8
{
	Empty,
	Active,
	Dying,
	Corpse,
	RefillPending,
	Suppressed,
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationSlotSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FName PopulationId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FName PopulationMemberId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") EAuraPopulationSlotState State = EAuraPopulationSlotState::Empty;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 Generation = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FString ActorNetworkPath;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") double TransitionServerTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") float RemainingCorpseDelay = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") float RemainingRefillDelay = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FString LastTransitionReason;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 DeathSequence = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationDebugSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 SchemaVersion = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 ManagerGeneration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FName MapId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FName DirectorPhase = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") FName BattleEventId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 ActiveCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 CorpseCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 PendingCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 SuppressedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") int32 MaximumCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Population|Snapshot") TArray<FAuraPopulationSlotSnapshot> Slots;
};

/** Stable, server-authored data carried by a spawned Civilian. */
USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationMemberState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName PopulationId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 PopulationSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName PopulationMemberId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName WorkProfileId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName ZoneId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName MerchantDefinitionId = NAME_None;

	bool IsValid() const
	{
		return !PopulationId.IsNone()
			&& PopulationSlotIndex >= 0
			&& !PopulationMemberId.IsNone()
			&& !WorkProfileId.IsNone()
			&& !ZoneId.IsNone();
	}

	bool operator==(const FAuraPopulationMemberState& Other) const
	{
		return PopulationId == Other.PopulationId
			&& PopulationSlotIndex == Other.PopulationSlotIndex
			&& PopulationMemberId == Other.PopulationMemberId
			&& WorkProfileId == Other.WorkProfileId
			&& ZoneId == Other.ZoneId
			&& MerchantDefinitionId == Other.MerchantDefinitionId;
	}

	bool operator!=(const FAuraPopulationMemberState& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct AURA_API FAuraCivilianWorkProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	FName WorkProfileId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float MovementSpeed = 120.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float WanderRadius = 300.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float ObserveRadius = 900.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float FleeDistance = 600.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float WorkDuration = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float WanderDuration = 8.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float CalmDuration = 5.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float MoveTimeout = 8.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float FleeMovementSpeed = 240.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	FGameplayTagContainer WorkMarkerTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	FGameplayTagContainer ObservationMarkerTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	FGameplayTagContainer ShelterMarkerTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float ScheduleStartHour = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	float ScheduleEndHour = 24.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Work")
	FName SchedulePhase = FName(TEXT("All"));
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationMemberOverride
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName WorkProfileId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName MerchantDefinitionId = NAME_None;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationRespawnPolicy
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Respawn")
	bool bEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Respawn")
	float DelaySeconds = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Respawn")
	float CorpseSeconds = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population|Respawn")
	TArray<FName> AllowedBattlePhases;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraPopulationSpawnRow
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName PopulationId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName MapId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName ZoneId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName RoleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FString ActorClassPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	TArray<FName> SpawnVolumeIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 InitialCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 MaximumCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FName DefaultWorkProfileId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	TArray<FAuraPopulationMemberOverride> MemberOverrides;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	bool bSpawnOnLoad = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	FAuraPopulationRespawnPolicy RespawnPolicy;
};
