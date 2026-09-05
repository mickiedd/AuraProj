// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AuraEnemyArchetypeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AuraGameplayDefinitionRegistry.generated.h"

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayMissionDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName ObjectiveType = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	TArray<FName> CellEncounterIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	int32 RunCapSeconds = 1200;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	int32 ExtractionSeconds = 30;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	bool bRequiresSurveyedAnchors = true;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName ArchetypeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	int32 SpawnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName LayoutAnchorId = NAME_None;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayEncounterPolicyDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition|Encounter")
	int32 MaxConcurrentHeavyAttacks = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition|Encounter")
	int32 MaxHeavyTokensPerParticipant = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition|Encounter")
	float HeavyCoverageRangeUnits = 600.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition|Encounter")
	int32 MaxDisruptors = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition|Encounter")
	FName SupportFieldOwnershipPolicy = NAME_None;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayEnemyArchetypeDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName CounterVerb = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	bool bRequiresInterrupt = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	bool bRequiresEvade = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	TArray<FAuraEnemyAttackDefinition> Attacks;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	bool bHasSupportField = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FAuraEnemySupportFieldDefinition SupportField;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	float FrontArcDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	float FrontDamageReductionFraction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	float SlamRecoverySeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayRoleLoadoutDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName RoleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	TArray<FName> AbilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	int32 EmergencyRefillCharges = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraGameplayArenaLayoutDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	bool bVerified = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Definition")
	TArray<FName> RequiredAnchors;
};

/** Loads immutable gameplay definitions and reports readiness separately from parse success. */
UCLASS()
class AURA_API UAuraGameplayDefinitionRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool LoadDefinitions(FString& OutError);
	bool AreDefinitionsLoaded() const { return bDefinitionsLoaded; }
	bool IsRuntimeReady() const { return bDefinitionsLoaded && bRuntimeReady; }
	const FString& GetRuntimeBlocker() const { return RuntimeBlocker; }
	const FString& GetDefinitionsHash() const { return DefinitionsHash; }

	const FAuraGameplayMissionDefinition* FindMission(FName Id) const { return Missions.Find(Id); }
	const FAuraGameplayEncounterDefinition* FindEncounter(FName Id) const { return Encounters.Find(Id); }
	const FAuraGameplayEnemyArchetypeDefinition* FindArchetype(FName Id) const { return Archetypes.Find(Id); }
	const FAuraGameplayRoleLoadoutDefinition* FindRoleLoadout(FName Id) const { return RoleLoadouts.Find(Id); }
	const FAuraGameplayArenaLayoutDefinition* FindLayout(FName Id) const { return Layouts.Find(Id); }
	const FAuraGameplayEncounterPolicyDefinition& GetEncounterPolicy() const { return EncounterPolicy; }
	int32 GetRoleLoadoutCount() const { return RoleLoadouts.Num(); }

private:
	bool LoadJsonObject(const FString& RelativePath, TSharedPtr<class FJsonObject>& OutObject, FString& OutError);
	bool CheckHeader(const TSharedPtr<class FJsonObject>& Object, const FString& RelativePath, FString& OutError);
	void ResetDefinitions();

	TMap<FName, FAuraGameplayMissionDefinition> Missions;
	TMap<FName, FAuraGameplayEncounterDefinition> Encounters;
	TMap<FName, FAuraGameplayEnemyArchetypeDefinition> Archetypes;
	FAuraGameplayEncounterPolicyDefinition EncounterPolicy;
	TMap<FName, FAuraGameplayRoleLoadoutDefinition> RoleLoadouts;
	TMap<FName, FAuraGameplayArenaLayoutDefinition> Layouts;
	FString DefinitionRevision;
	FString DefinitionsHash;
	FString RuntimeBlocker;
	bool bDefinitionsLoaded = false;
	bool bRuntimeReady = false;
};
