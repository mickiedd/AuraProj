// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Runtime facts shared by the Day 19 development-only cross-process probe. */
struct AURA_API FAuraRoleBattleNetworkProbeSnapshot
{
	bool bAuthorityWorld = false;
	bool bWorldReady = false;
	bool bPopulationReady = false;
	bool bEconomyReady = false;
	bool bFireGunConfigured = false;
	int32 PlayerControllers = 0;
	int32 RemoteClients = 0;
	int32 Civilians = 0;
	int32 Enemies = 0;
	int32 ActiveMerchants = 0;
	int32 ConfiguredPopulationSlots = 0;
	int32 ConfiguredEnemyRows = 0;

	bool HasRequiredFixture() const
	{
		return bAuthorityWorld && bWorldReady && bPopulationReady && bEconomyReady
			&& bFireGunConfigured && RemoteClients >= 2 && ActiveMerchants >= 1;
	}

	bool MeetsPerformanceFixtureMinimum() const
	{
		return ConfiguredPopulationSlots >= 25 && ConfiguredEnemyRows >= 5 && ActiveMerchants >= 1;
	}
};

/** Development/automation inspection helpers; no client mutation path is exposed. */
class AURA_API FAuraRoleBattleNetworkProbe final
{
public:
	static bool CollectServerSnapshot(const UWorld* World, FAuraRoleBattleNetworkProbeSnapshot& OutSnapshot, FString& OutError);
	static void LogServerSnapshot(const FAuraRoleBattleNetworkProbeSnapshot& Snapshot);
	static constexpr int32 ReplayCacheLimit = 256;
};
