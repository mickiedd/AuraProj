// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraPopulationTypes.h"
#include "AuraPopulationManager.generated.h"

class AAuraCivilian;
class AAuraCivilianSpawnVolume;
class AAuraCivilianActivityMarker;
class UAuraPopulationSpawnDefinition;
class UAuraCivilianWorkProfileRegistry;
class UAuraBattleZoneConfig;
struct FAuraDeathEvent;
enum class EAuraCombatLifeState : uint8;
enum class EAuraBattlePhase : uint8;

struct FAuraPopulationRuntimeSlot
{
	FAuraPopulationMemberState Member;
	EAuraPopulationSlotState State = EAuraPopulationSlotState::Empty;
	TWeakObjectPtr<AAuraCivilian> Actor;
	int32 Generation = 0;
	int32 DeathSequence = 0;
	double TransitionServerTime = 0.0;
	FString LastTransitionReason;
	FTimerHandle CorpseTimer;
	FTimerHandle RefillTimer;
	int32 RetryCount = 0;
	FDelegateHandle LifeStateDelegateHandle;
};

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
	bool FinalizeInitialPopulation();
	bool ValidateBattleZoneRegistrations(const UAuraBattleZoneConfig* ZoneConfig, FString& OutError) const;
	void Shutdown();
	FAuraPopulationDebugSnapshot BuildDebugSnapshot() const;

	bool IsInitialized() const { return bDefinitionsInitialized; }
	bool IsFinalized() const { return bInitialPopulationFinalized; }
	int32 GetInitializationGeneration() const { return InitializationGeneration; }
	int32 GetLiveMemberCount() const { return LiveMembers.Num(); }
	int32 GetRegisteredSpawnVolumeCount() const { return RegisteredVolumes.Num(); }
	int32 GetRegisteredActivityMarkerCount() const { return RegisteredActivityMarkers.Num(); }
	const TArray<FAuraPopulationSpawnRow>& GetPopulationRows() const { return PopulationRows; }
	FName GetCurrentMapId() const { return FindCurrentMapId(); }
	const FAuraCivilianWorkProfile* FindWorkProfile(FName WorkProfileId) const;
	void GetLiveMembers(TArray<AAuraCivilian*>& OutMembers) const;

	void RegisterActivityMarker(AAuraCivilianActivityMarker* Marker);
	void UnregisterActivityMarker(AAuraCivilianActivityMarker* Marker);
	AAuraCivilianActivityMarker* FindActivityMarker(FName MarkerId) const;
	void GetActivityMarkers(TArray<AAuraCivilianActivityMarker*>& OutMarkers) const;
	bool TryReserveActivityMarker(FName MarkerId, FName PopulationMemberId);
	void ReleaseActivityMarkerReservation(FName MarkerId, FName PopulationMemberId);
	void ReleaseAllActivityMarkerReservations(FName PopulationMemberId);
	void HandlePopulationDeath(const FAuraDeathEvent& Event);
	int32 GetRecordedPopulationDeathCount() const { return RecordedPopulationDeathCount; }
	int32 GetTrackedCorpseTimerCount() const;
	int32 GetTrackedRefillTimerCount() const;

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
	bool SpawnMember(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, const TCHAR* Reason);
	bool ResolveMemberState(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, FAuraPopulationMemberState& OutState) const;
AAuraCivilianSpawnVolume* FindVolumeForRow(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, int32 AttemptIndex) const;
	void RollBackInitialPopulation();
	const FAuraPopulationSpawnRow* FindRow(FName PopulationId) const;
	bool IsRefillAllowed(const FAuraPopulationSpawnRow& Row) const;
	void TransitionSlot(FAuraPopulationRuntimeSlot& Slot, EAuraPopulationSlotState NewState, const TCHAR* Reason);
	void BindMemberLifeState(FAuraPopulationRuntimeSlot& Slot, AAuraCivilian* Civilian);
	void UnbindMemberLifeState(FAuraPopulationRuntimeSlot& Slot);
	void HandleMemberLifeStateChanged(FName MemberId, int32 ExpectedGeneration, EAuraCombatLifeState NewState);
	void ScheduleCorpseCleanup(FAuraPopulationRuntimeSlot& Slot, const FAuraPopulationSpawnRow& Row);
	void ExecuteCorpseCleanup(FName MemberId, int32 ExpectedGeneration, int32 ExpectedDeathSequence);
	void ScheduleRefill(FAuraPopulationRuntimeSlot& Slot, const FAuraPopulationSpawnRow& Row, float DelaySeconds, const TCHAR* Reason);
	void ExecuteRefill(FName MemberId, int32 ExpectedGeneration);
	void HandleBattlePhaseChanged(EAuraBattlePhase NewPhase);

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
	TMap<FName, TObjectPtr<AAuraCivilianActivityMarker>> RegisteredActivityMarkers;
	TMap<FName, TSet<FName>> ActivityMarkerReservations;

	UPROPERTY(Transient)
	TObjectPtr<UAuraCivilianWorkProfileRegistry> WorkProfileRegistry;

	TMap<FName, TWeakObjectPtr<AAuraCivilian>> LiveMembers;
	TMap<TWeakObjectPtr<AActor>, FName> ActorToMember;
	TSet<FName> ReservedMemberIds;
	/** Number of accepted authoritative death events; repeated deaths of one stable member count separately. */
	int32 RecordedPopulationDeathCount = 0;
	TMap<FName, FAuraPopulationRuntimeSlot> RuntimeSlots;
	TWeakObjectPtr<class AAuraBattleDirector> BoundBattleDirector;
	FDelegateHandle PhaseChangedDelegateHandle;

	bool bDefinitionsInitialized = false;
	bool bInitialPopulationFinalized = false;
	bool bShuttingDown = false;
	int32 InitializationGeneration = 0;
};
