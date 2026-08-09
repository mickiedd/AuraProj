# Day 19 - Harden Multiplayer Behavior

## Goal

Prove the complete first vertical slice under server authority in both mandatory network topologies: a listen server with two remote clients, and a dedicated server with two remote clients. Close security, replication, late-join, reconnect, concurrency, replay, privacy, lifecycle, and bounded-performance gaps with a repeatable cross-process runner.

The checked-in `AuraServer.Target.cs` makes the dedicated matrix required. Failure to build or launch that target blocks this day; it is not an optional environment skip.

## Prerequisite gate

- Every Day 02-18 completion gate, focused suite, and required network smoke is green at one recorded revision.
- The Day 18 two-player isolation/reconnect and load-once world restoration tests pass from clean automation saves.
- The final fixture map has deterministic population IDs, two validated test profiles, and no unresolved configuration error.
- The production authenticated Online Subsystem provider chosen on Day 18 is recorded; automation fixture identities remain compile-gated and are never accepted by Shipping.

## New files

- Source/Aura/Public/Tests/AuraRoleBattleNetworkProbe.h
- Source/Aura/Private/Tests/AuraRoleBattleNetworkProbe.cpp
- Source/Aura/Private/Tests/AuraRoleBattleMultiplayerTests.cpp
- Content/AutoTests/RoleBattleDay19VerticalSlice.xml
- Content/Maps/Tests/RoleBattleDay19.umap
- RunRoleBattleDay19Multiplayer.ps1
- Docs/Reports/Role-Battle-Day-19-Multiplayer.md

Test probes and commands must compile only with development/automation support and expose no Shipping mutation path.

## Exact runtime files to inspect or modify

Combat and lifecycle:

- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/Combat/AuraCombatRules.h
- Source/Aura/Private/Combat/AuraCombatRules.cpp
- Source/Aura/Public/Combat/AuraCombatStateComponent.h
- Source/Aura/Private/Combat/AuraCombatStateComponent.cpp
- Source/Aura/Public/Combat/AuraTargetableInterface.h
- Source/Aura/Private/Combat/AuraTargetableInterface.cpp
- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Character/AuraEnemy.h
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AbilitySystem/AuraAttributeSet.h
- Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp
- Source/Aura/Private/Actor/AuraProjectile.cpp
- Source/Aura/Private/Actor/AuraFireBall.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp

AI, battle, population, and interaction:

- Source/Aura/Public/AI/AuraCivilianAIController.h
- Source/Aura/Private/AI/AuraCivilianAIController.cpp
- Source/Aura/Public/AI/BTService_FindNearestHostile.h
- Source/Aura/Private/AI/BTService_FindNearestHostile.cpp
- Source/Aura/Public/AI/BTService_FindNearestThreat.h
- Source/Aura/Private/AI/BTService_FindNearestThreat.cpp
- Source/Aura/Public/Battle/AuraBattleDirector.h
- Source/Aura/Private/Battle/AuraBattleDirector.cpp
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp
- Source/Aura/Public/World/AuraPopulationSpawnDefinition.h
- Source/Aura/Private/World/AuraPopulationSpawnDefinition.cpp
- Source/Aura/Public/World/AuraCivilianSpawnVolume.h
- Source/Aura/Private/World/AuraCivilianSpawnVolume.cpp
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp

Economy, persistence, and merchant UI:

- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraEconomyRegistrySubsystem.h
- Source/Aura/Private/Economy/AuraEconomyRegistrySubsystem.cpp
- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp
- Source/Aura/Public/Economy/AuraCommerceSubsystem.h
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp
- Source/Aura/Public/Game/AuraPersistenceSubsystem.h
- Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp
- Source/Aura/Public/Game/AuraPlayerProfileIdentity.h
- Source/Aura/Private/Game/AuraPlayerProfileIdentity.cpp
- Source/Aura/Public/Game/AuraPersistenceManifestSaveGame.h
- Source/Aura/Private/Game/AuraPersistenceManifestSaveGame.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Game/AuraGameInstance.h
- Source/Aura/Private/Game/AuraGameInstance.cpp
- Source/Aura/Public/Game/LoginPlayerController.h
- Source/Aura/Private/Game/LoginPlayerController.cpp
- Source/Aura/Public/Game/LoadingPlayerController.h
- Source/Aura/Private/Game/LoadingPlayerController.cpp
- Source/Aura/Public/Game/GameServerClient.h
- Source/Aura/Private/Game/GameServerClient.cpp
- Source/Aura/Public/Game/ServerTravelComponent.h
- Source/Aura/Private/Game/ServerTravelComponent.cpp
- Source/Aura/Public/Game/AuraPlayerSaveGame.h
- Source/Aura/Private/Game/AuraPlayerSaveGame.cpp
- Source/Aura/Public/Game/AuraWorldSaveGame.h
- Source/Aura/Private/Game/AuraWorldSaveGame.cpp
- Source/Aura/Public/UI/HUD/AuraHUD.h
- Source/Aura/Private/UI/HUD/AuraHUD.cpp
- Source/Aura/Public/UI/Widget/AuraMerchantWidget.h
- Source/Aura/Private/UI/Widget/AuraMerchantWidget.cpp
- Source/Aura/Public/UI/WidgetController/MerchantWidgetController.h
- Source/Aura/Private/UI/WidgetController/MerchantWidgetController.cpp

Test infrastructure and fixtures:

- Source/Aura.Target.cs
- Source/AuraServer.Target.cs
- Source/Aura/Aura.Build.cs
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Public/AutoTestAgent.h
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Private/AutoTestAgent.cpp
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Public/AutoTestNodes.h
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Private/AutoTestNodes.cpp
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Public/AutoTestResults.h
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Private/AutoTestReportWriter.cpp
- Plugins/AuraAutoTest/AuraAutoTestPlugin.uplugin
- Config/DefaultGame.ini
- Content/Config/RoleConfig.json
- Content/Config/PopulationSpawnTable.json
- Content/Config/BattleZones.json
- Content/Config/ItemDefinitions.json
- Content/Config/MerchantDefinitions.json
- Content/Config/EconomyConfig.json
- Content/Blueprints/UI/Merchant/WBP_Merchant.uasset
- Content/Blueprints/UI/Merchant/WBP_MerchantOfferRow.uasset
- Content/Blueprints/UI/WidgetController/BP_MerchantWidgetController.uasset

## Cross-process harness

1. Extend the existing hidden-process network-smoke pattern rather than relying on one in-process AutoTest runner world.
2. Build `AuraEditor Win64 Development` and `AuraServer Win64 Development` before launching tests.
3. Run both mandatory matrices:
   - Listen server host plus two independent remote client processes.
   - Dedicated server process plus two independent remote client processes.
4. Give the two clients distinct server-approved fixture profile identities and isolated user/save directories. Reuse each validated identity for its reconnect phase.
5. Use isolated ports, absolute per-process logs, a bounded timeout, deterministic barriers for concurrent actions, and unique artifact directories.
6. Stop only processes launched by the runner. Any crash, timeout, missing assertion, skipped topology, early process exit, server `ensure`/fatal error, or nonzero child result makes the runner fail.
7. Aggregate server and both client assertions into machine-readable JSON plus the checked-in Markdown report. Record engine version, revision, topology, commands, ports, fixture IDs, logs, and failures.
   The runner returns nonzero on any assertion/process/crash/timeout/missing-artifact failure and writes `Saved/Logs/Day19-{Listen|Dedicated}-{Server|Client1|Client2}.log`, `Saved/Reports/Day19-{Listen|Dedicated}.json`, and `Docs/Reports/Role-Battle-Day-19-Multiplayer.md`.
8. Repeat replay, last-stock contention, and result-correlation cases under a recorded network-emulation profile of 100 ms packet lag, 20 ms variance, and 2 percent packet loss. The test must eventually terminate with the same authoritative invariants and no duplicate commit.

## Exact execution commands

Run from the repository root after the Day 18 prerequisite gate is recorded:

```powershell
& '.\build_test.bat'
& '.\BuildDedicatedServer.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day19Automation.log'

& '.\RunRoleBattleDay19Multiplayer.ps1' -Mode Listen
& '.\RunRoleBattleDay19Multiplayer.ps1' -Mode Dedicated
```

The two mode invocations are mandatory; a single topology pass, an in-process-only AutoTest run, or a log-only result does not satisfy the Day 19 gate.

## Functional fixture

Each topology contains:

- Two remote Players with distinct persistent profiles; the listen topology also has its host.
- At least one Enemy used for combat and reward tests.
- At least five ordinary Civilians.
- One bound Civilian merchant with a finite-stock offer.
- Protected and conflict-enabled civilian zones.
- A deterministic obstruction for line-of-sight rejection.
- Canonical `PopulationMemberId` values and a clean isolated world save.

## Required security and replication matrix

From a non-authority remote client, attempt and prove server rejection/no state change for:

- Unapproved role selection or client-authored role asset/path.
- Direct ability grant or activation of an ungranted offensive ability.
- Client-authored damage, target substitution, friendly damage, protected-civilian damage, and damage after death.
- Direct wallet/inventory mutation and the non-Shipping grant command.
- Forged price, item ID, quantity, stock, eligibility, merchant binding, and nonexistent offer.
- Purchase outside range, without line of sight, from a dead/unavailable merchant, or with insufficient funds/capacity.
- Wrong session nonce, skipped request ID, exact replay, stale request outside the bounded cache, and an old nonce after reconnect.

Also prove:

- Role, identity, health, combat/death state, battle phase, civilian AI state, merchant availability/offers/stock, and the requesting player's economy result replicate correctly.
- Client A receives no wallet or inventory contents from Client B's PlayerState.
- A late-joining client sees current death, battle, population, merchant-stock, and availability state rather than initial defaults.
- Client display JSON/UI state cannot change the server's accepted transaction values.

## Lifecycle, reconnect, and concurrency matrix

1. Disconnect/reconnect the same validated profile during combat, after death, and after a purchase committed but before its Client result is observed.
2. After reconnect, assert exact role/wallet/inventory restoration, a new session nonce, rejection of the old nonce, and no duplicate item/currency/stock mutation.
3. Apply simultaneous lethal damage from two sources and assert one death transition, one event, and at most one configured reward.
4. Barrier-release two clients to buy the last stock unit; assert one success, one sold-out result, and conservation of currency/items/stock.
5. Send multiple ordered requests from one player and exact replays from both clients; assert deterministic results and a maximum replay cache of 256 entries per active session.
6. Destroy/clean up a dead civilian while clients target or interact with it; pointers, prompts, and UI must clear without crash or stale purchase.
7. Save/reload after purchase and civilian death; population and merchant stock restore once and player records remain isolated.

## Mandatory acceptance tests

- Player-to-Enemy and Enemy-to-Player damage succeed through shared rules.
- Player-to-Player and Enemy-to-Enemy friendly damage fail.
- Allowed Enemy/Player-to-Civilian damage succeeds only in the configured conflict rule.
- Protected civilian damage fails.
- Projectile, beam, radial, hitscan, and melee paths obey the same result.
- Civilian death produces one event and no default enemy reward.
- Player respawn and enemy reward/respawn behavior remain correct exactly once.
- Valid purchase commits once; every failure and replay leaves authoritative state correct.
- Owner-only wallet/inventory privacy passes with two remote clients.
- Late join and stable-profile reconnect receive current state.
- Forged client data and direct authority-bypass attempts never change server state.
- Both listen and dedicated matrices pass with no skipped required assertion.

## Performance and bounded-state gate

1. In a Development non-editor server run, use a fixed performance fixture of 25 civilians, five enemies, one merchant, and two remote clients.
2. Warm up for 30 seconds and sample for 60 seconds. Record reference hardware, build, map, command line, server frame/game-thread data, actor counts, memory, and per-client bandwidth.
3. Required thresholds:
   - Server game-thread p95 at or below 33.3 ms and p99 at or below 50 ms.
   - After warm-up, server working-set growth during the 60-second sample is at most 128 MiB and has no sustained upward trend greater than 1 MiB per minute in a repeated five-minute soak.
   - Per remote client, mean received gameplay bandwidth is at most 256 KiB/s and one-second-window p95 is at most 512 KiB/s in the fixed fixture.
   - Active civilian/enemy counts never exceed configured maxima and do not drift without a lifecycle event.
   - Replay cache never exceeds 256 terminal entries per active player session and is released on session expiry.
   - After 1,000 forged/replayed purchase requests, authoritative economy state is unchanged and retained commerce bookkeeping has no unbounded growth.
4. A threshold miss blocks completion or must be resolved by an explicitly approved scope/budget change; merely recording the miss is insufficient.

## Completion gate

The checked-in cross-process runner passes every required assertion in both listen and dedicated topologies with two remote clients, meets the performance/bounded-state thresholds, produces complete artifacts, and leaves no known client authority bypass or player-state privacy leak.
