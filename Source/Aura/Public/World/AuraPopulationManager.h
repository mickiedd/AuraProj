// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraPopulationTypes.h"
#include "AuraPopulationManager.generated.h"

class AAuraCivilian;
class AAuraCivilianSpawnVolume;
class UAuraPopulationSpawnDefinition;

/**
 * Authority-only owner of Day 09 population definitions, slots and live actors.
 * It is transient and is created by AAuraGameModeBase before StartPlay.
 */
UCLASS(Transient)
class AURA_API UAuraPopulationManager : public UObject
{
	GENERATED_BODY()

public:
	bool InitializeDefinitions(FString& OutError);
	void RegisterSpawnVolume(AAuraCivilianSpawnVolume* Volume);
	void UnregisterSpawnVolume(AAuraCivilianSpawnVolume* Volume);
	void ScheduleInitialPopulation();
	bool FinalizeInitialPopulation();

	bool IsInitialized() const { return bDefinitionsInitialized; }
	bool IsFinalized() const { return bInitialPopulationFinalized; }
	int32 GetInitializationGeneration() const { return InitializationGeneration; }
	int32 GetLiveMemberCount() const { return LiveMembers.Num(); }
	const TArray<FAuraPopulationSpawnRow>& GetPopulationRows() const { return PopulationRows; }
	FName GetCurrentMapId() const { return FindCurrentMapId(); }

	/** Canonical identity used by initial spawn and all future refill work. */
	static FName BuildDeterministicMemberId(FName PopulationId, int32 SlotIndex);
	/** Population rows may only instantiate the dedicated non-combat Civilian class. */
	static bool IsValidCivilianActorClass(const UClass* ActorClass);

	/** Test/debug lookup; no client mutation path is exposed. */
	const AAuraCivilian* FindLiveMember(FName PopulationMemberId) const;

private:
	bool IsCurrentMapRow(const FAuraPopulationSpawnRow& Row) const;
	FName FindCurrentMapId() const;
	bool ValidateRoleAndClass(const FAuraPopulationSpawnRow& Row, FString& OutError) const;
	bool SpawnInitialMember(const FAuraPopulationSpawnRow& Row, int32 SlotIndex);
	bool ResolveMemberState(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, FAuraPopulationMemberState& OutState) const;
	AAuraCivilianSpawnVolume* FindVolumeForRow(const FAuraPopulationSpawnRow& Row, int32 AttemptIndex) const;

	UFUNCTION()
	void OnMemberDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<UAuraPopulationSpawnDefinition> Definition;

	UPROPERTY(Transient)
	TArray<FAuraPopulationSpawnRow> PopulationRows;

	UPROPERTY(Transient)
	TArray<FAuraCivilianWorkProfile> WorkProfiles;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AAuraCivilianSpawnVolume>> RegisteredVolumes;

	TMap<FName, TWeakObjectPtr<AAuraCivilian>> LiveMembers;
	TMap<TWeakObjectPtr<AActor>, FName> ActorToMember;
	TSet<FName> ReservedMemberIds;

	bool bDefinitionsInitialized = false;
	bool bInitialPopulationFinalized = false;
	bool bFinalizeScheduled = false;
	int32 InitializationGeneration = 0;
};
