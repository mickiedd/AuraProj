// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Battle/AuraBattleZoneTypes.h"
#include "Combat/AuraCombatRuleContext.h"
#include "AuraBattleDirector.generated.h"

class UAuraBattleZoneConfig;
struct FAuraDeathEvent;

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraBattlePhaseChanged, EAuraBattlePhase);

/** Authority-owned battle phase and location-policy resolver. */
UCLASS()
class AURA_API AAuraBattleDirector : public AInfo
{
	GENERATED_BODY()

public:
	AAuraBattleDirector();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EAuraBattlePhase GetCurrentPhase() const { return CurrentPhase; }
	FName GetActiveBattleEventId() const { return ActiveBattleEventId; }
	int32 GetConfigVersion() const { return ConfigVersion; }
	const FString& GetConfigHash() const { return ConfigHash; }
	int32 GetPopulationActiveCount() const { return PopulationActiveCount; }
	int32 GetPopulationMaximumCount() const { return PopulationMaximumCount; }
	int32 GetPopulationPendingCount() const { return PopulationPendingCount; }
	int32 GetPopulationCasualtyCount() const { return PopulationCasualtyCount; }
	const UAuraBattleZoneConfig* GetZoneConfig() const { return ZoneConfig; }
	bool IsAuthorityConfigurationReady() const { return bAuthorityConfigurationReady; }
	const FString& GetInitializationError() const { return InitializationError; }

	bool TransitionTo(EAuraBattlePhase NewPhase);
	/** Publishes an authority-owned population summary for player-facing battle HUDs. */
	void UpdatePopulationSummary(int32 ActiveCount, int32 MaximumCount, int32 PendingCount, int32 CasualtyCount);
	bool ResolveCombatRuleContext(const AActor* SourceActor, const AActor* TargetActor, FAuraCombatRuleContext& OutContext) const;

	FAuraBattlePhaseChanged OnPhaseChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentPhase();

	UFUNCTION()
	void OnRep_ActiveBattleEventId();

	void HandleAuthoritativeDeath(const FAuraDeathEvent& Event);

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle")
	EAuraBattlePhase CurrentPhase = EAuraBattlePhase::Peace;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveBattleEventId, VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle")
	FName ActiveBattleEventId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle")
	int32 ConfigVersion = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle")
	FString ConfigHash;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Population")
	int32 PopulationActiveCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Population")
	int32 PopulationMaximumCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Population")
	int32 PopulationPendingCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Population")
	int32 PopulationCasualtyCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAuraBattleZoneConfig> ZoneConfig;

	bool bAuthorityConfigurationReady = false;
	FString InitializationError;
	int32 BattleEventSequence = 0;
};
