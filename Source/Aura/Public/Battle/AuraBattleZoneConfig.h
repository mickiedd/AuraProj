// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Battle/AuraBattleZoneTypes.h"
#include "Combat/AuraCombatRuleContext.h"
#include "AuraBattleZoneConfig.generated.h"

class UWorld;

/** Validated, immutable server-side view of Content/Config/BattleZones.json. */
UCLASS(Transient)
class AURA_API UAuraBattleZoneConfig : public UObject
{
	GENERATED_BODY()

public:
	bool Load(FString& OutError);
	const FAuraBattleZoneDefinition* ResolveZone(const FVector& Location, FName MapId) const;
	FAuraCombatPolicySnapshot BuildPolicySnapshot(const UObject* WorldContext, const FVector& Location, EAuraBattlePhase Phase, FName EventId) const;
	FAuraCombatPolicySnapshot BuildPolicySnapshotForMap(const UObject* WorldContext, const FVector& Location, FName MapId, EAuraBattlePhase Phase, FName EventId) const;
	int32 GetSchemaVersion() const { return SchemaVersion; }
	const FString& GetConfigHash() const { return ConfigHash; }
	const TArray<FAuraBattleZoneDefinition>& GetZones() const { return Zones; }

private:
	static FString GetConfigPath();
	static FName ResolveCurrentMapId(const UObject* WorldContext);

	UPROPERTY(Transient)
	TArray<FAuraBattleZoneDefinition> Zones;

	int32 SchemaVersion = 0;
	FString ConfigHash;
};
