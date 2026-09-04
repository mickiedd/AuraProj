// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/AuraMissionTypes.h"
#include "AuraMissionSubsystem.generated.h"

class AAuraMissionState;
class AAuraPlayerState;
class UAuraEncounterCoordinator;
class UAuraGameplayDefinitionRegistry;

/**
 * Authority-only mission owner. It is deliberately inert until both the
 * definition registry and the inherited Day 40/Day 41 entry gate are ready.
 */
UCLASS()
class AURA_API UAuraMissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Loads the new profile but refuses activation while survey/evidence gates are missing. */
	bool EnableGameplayProfile(FString& OutError);
	bool IsGameplayProfileEnabled() const { return bGameplayProfileEnabled; }
	bool IsRuntimeEntryReady() const { return bRuntimeEntryReady; }
	const FString& GetEntryBlocker() const { return EntryBlocker; }

	const FAuraMissionRunState& GetRunState() const { return RunState; }
	const AAuraMissionState* GetMissionStateActor() const { return MissionStateActor; }
	const UAuraGameplayDefinitionRegistry* GetDefinitionRegistry() const { return DefinitionRegistry; }
	const UAuraEncounterCoordinator* GetEncounterCoordinator() const { return EncounterCoordinator; }

private:
	bool IsAuthority() const;

	UPROPERTY(Transient)
	TObjectPtr<AAuraMissionState> MissionStateActor;

	UPROPERTY(Transient)
	TObjectPtr<UAuraGameplayDefinitionRegistry> DefinitionRegistry;

	UPROPERTY(Transient)
	TObjectPtr<UAuraEncounterCoordinator> EncounterCoordinator;

	FAuraMissionRunState RunState;
	FString EntryBlocker;
	bool bGameplayProfileEnabled = false;
	bool bRuntimeEntryReady = false;
};
