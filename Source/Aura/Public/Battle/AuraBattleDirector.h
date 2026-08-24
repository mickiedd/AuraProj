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
	const UAuraBattleZoneConfig* GetZoneConfig() const { return ZoneConfig; }

	bool TransitionTo(EAuraBattlePhase NewPhase);
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

	UPROPERTY(Transient)
	TObjectPtr<UAuraBattleZoneConfig> ZoneConfig;

	int32 BattleEventSequence = 0;
};
