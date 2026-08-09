# Day 20 - Finish the First Vertical Slice

## Goal

First remove transitional gameplay/security assumptions and document intentional limitations. Only after cleanup is complete, perform the full rebuild, native/plugin automation, cook, package, staged-data validation, and packaged listen/dedicated regression required to call the vertical slice releasable.

## Prerequisite gate

- Day 19's mandatory listen and dedicated matrices, artifacts, security/privacy assertions, and performance thresholds pass at the candidate revision.
- No Day 02-19 completion gate is waived or represented only by a manual/log-only observation where an assertion is required.
- Begin Day 20 from a recorded candidate revision; cleanup changes create a new revision that must pass the entire Phase B pipeline again.

## Exact code files to inspect or modify

Combat, targeting, roles, and lifecycle:

- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AbilitySystem/AuraAttributeSet.h
- Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp
- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Character/AuraEnemy.h
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Source/Aura/Private/AI/BTService_FindNearestHostile.cpp
- Source/Aura/Private/AI/BTService_FindNearestThreat.cpp
- Source/Aura/Private/Actor/AuraProjectile.cpp
- Source/Aura/Private/Actor/AuraFireBall.cpp
- Source/Aura/Public/Actor/AuraEffectActor.h
- Source/Aura/Private/Actor/AuraEffectActor.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp

The Day 10-deleted paths `Source/Aura/Public/AI/BTService_FindNearestPlayer.h` and `Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp` must remain absent, and source/config/snapshot/asset-reference scans must remain clean. Do not recreate the legacy service merely to satisfy this audit manifest.

Economy, persistence, and development surfaces:

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
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp
- Source/Aura/Public/Game/AuraPersistenceSubsystem.h
- Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp
- Source/Aura/Public/Game/AuraPlayerProfileIdentity.h
- Source/Aura/Private/Game/AuraPlayerProfileIdentity.cpp
- Source/Aura/Public/Game/AuraPlayerSaveGame.h
- Source/Aura/Private/Game/AuraPlayerSaveGame.cpp
- Source/Aura/Public/Game/AuraWorldSaveGame.h
- Source/Aura/Private/Game/AuraWorldSaveGame.cpp
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
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp

## Content, packaging, tests, and documentation to inspect or modify

- Config/DefaultGame.ini
- Aura.uproject
- Source/Aura.Target.cs
- Source/AuraEditor.Target.cs
- Source/AuraServer.Target.cs
- Source/Aura/Aura.Build.cs
- Plugins/AuraAbilityGraph/AuraAbilityGraph.uplugin
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/AuraAbilityGraph.Build.cs
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraphEditor/AuraAbilityGraphEditor.Build.cs
- Plugins/AuraAutoTest/AuraAutoTestPlugin.uplugin
- Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/AuraAutoTestRuntime.Build.cs
- Plugins/AuraAutoTest/Source/AuraAutoTestEditor/AuraAutoTestEditor.Build.cs
- Plugins/BehaviorU/BehaviorUPlugin.uplugin
- Plugins/BehaviorU/Source/BehaviorURuntime/BehaviorURuntime.Build.cs
- Plugins/BehaviorU/Source/BehaviorUEditor/BehaviorUEditor.Build.cs
- Content/Config/RoleConfig.json
- Content/Config/PopulationSpawnTable.json
- Content/Config/BattleZones.json
- Content/Config/CivilianWorkProfiles.json
- Content/Config/ItemDefinitions.json
- Content/Config/MerchantDefinitions.json
- Content/Config/EconomyConfig.json
- Content/Config/PickupDefinitions.json
- Content/Config/ServerConnection.json
- Content/AbilityDefinitions
- Content/Maps/Tests/RoleBattleDay19.umap
- Content/Blueprints/UI/Merchant/WBP_Merchant.uasset
- Content/Blueprints/UI/Merchant/WBP_MerchantOfferRow.uasset
- Content/Blueprints/UI/WidgetController/BP_MerchantWidgetController.uasset
- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp
- Source/Aura/Private/Tests/AuraEconomyConfigTests.cpp
- Source/Aura/Private/Tests/AuraEconomyStateTests.cpp
- Source/Aura/Private/Tests/AuraCommerceTests.cpp
- Source/Aura/Private/Tests/AuraPersistenceTests.cpp
- Source/Aura/Private/Tests/AuraRoleBattleMultiplayerTests.cpp
- Content/AutoTests/RoleBattleDay16OwnerReplication.xml
- Content/AutoTests/RoleBattleDay17Merchant.xml
- Content/AutoTests/RoleBattleDay18Persistence.xml
- Content/AutoTests/RoleBattleDay19VerticalSlice.xml
- build_test.bat
- BuildDedicatedServer.bat
- build_smoke.bat
- RunSmokeTest.bat
- RunRoleBattleDay1Smoke.bat
- RunRoleBattleDay2NetworkSmoke.ps1
- RunRoleBattleDay3NetworkSmoke.ps1
- RunRoleBattleDay4DamageSmoke.ps1
- RunRoleBattleDay5ConfigSmoke.ps1
- RunRoleBattleDay6NetworkSmoke.ps1
- RunRoleBattleDay7ListenSmoke.ps1
- RunRoleBattleDay7DedicatedSmoke.ps1
- RunRoleBattleDay7PackagedSmoke.ps1
- RunRoleBattleDay8NetworkSmoke.ps1
- RunRoleBattleDay9NetworkSmoke.ps1
- RunRoleBattleDay10NetworkSmoke.ps1
- RunRoleBattleDay11NetworkSmoke.ps1
- RunRoleBattleDay12NetworkSmoke.ps1
- RunRoleBattleDay13NetworkSmoke.ps1
- RunRoleBattleDay14NetworkSmoke.ps1
- RunRoleBattleDay15NetworkSmoke.ps1
- RunRoleBattleDay16NetworkSmoke.ps1
- RunRoleBattleDay17NetworkSmoke.ps1
- RunRoleBattleDay18PersistenceSmoke.ps1
- RunRoleBattleDay19Multiplayer.ps1
- Docs/Tracking/GAS-Migration-TODOs.md
- Docs/Reports/Role-Battle-Day-19-Multiplayer.md
- Docs/Plans/Role-Creation-and-Battle-System-Plan.md
- Docs/Plans/Role-Battle-Implementation-Index.md
- Docs/README.md
- Docs/Reference/Role-Battle-Economy-Schema.md
- Docs/Reference/Role-Battle-Vertical-Slice-Test-Procedure.md
- Docs/Reports/Role-Battle-Vertical-Slice-{completion-date}.md

`{completion-date}` is replaced with the actual ISO completion date when the report is created.

### Closeout-document exception

The implementation index normally says to change only files listed for a day. This Day 20 manifest explicitly authorizes the master plan, implementation index, README, schema/test references, TODO tracker, and dated final report because synchronizing authoritative status is part of the release output. It does not authorize unrelated feature work. Planning/audit edits may revise contracts in these closeout documents, but no document may be marked implemented until the Phase A and Phase B evidence gates below pass.

## Phase A - Cleanup before release testing

1. Search all runtime C++, plugin action nodes, Blueprint snapshots, and Blueprint assets for direct Player/Enemy/Civilian actor-tag combat decisions, class-cast faction decisions, `IsNotFriend`, direct health mutation, and damage application that bypasses `AuraCombatRules`.
2. Replace safe remaining cases with combat identity/rules access. Keep a compatibility wrapper only when all callers delegate to the shared rule and mark its removal status explicitly.
3. Resolve `AuraEffectActor::bApplyEffectsToEnemies` through an explicit pickup-eligibility policy. Preserve the current Player/Enemy behavior unless a deliberate, separately tested design change is approved; do not accidentally classify Civilian as Enemy or as universally pickup-eligible.
4. Verify projectile, FireBall, beam, radial, hitscan, melee, and every AuraAbilityGraph damage action performs the final authority-side permission check and correct source/ability attribution.
5. Search every role application and load path. Previous role state must clear before new state is applied, duplicate grants must remain impossible, and runtime hot-swapping stays disabled until transactional ASC cleanup exists.
6. Search every Civilian path for player respawn, enemy loot/XP, enemy AI, and enemy spawn-table assumptions. Search death/reward paths for duplicate effects.
7. Audit every economy mutation UFUNCTION, RPC, console command, Blueprint node, test hook, and subsystem entry point. Shipping builds must expose only the player-owned validated purchase RPC; direct grant/mutation/test endpoints must be absent.
8. Retain useful automation fixtures behind `WITH_DEV_AUTOMATION_TESTS` or non-Shipping compile guards. Remove production-accessible debug grants, verbose logs, and cheats; do not delete the repeatable test harness merely because it is development-only.
9. Confirm replay caches, delegates, timers, interaction targets, UI references, and population/merchant registrations are released on disconnect, death, end play, travel, and subsystem shutdown.
10. Validate all JSON schema fields and produce `Docs/Reference/Role-Battle-Economy-Schema.md`. JSON files themselves do not support comments; put comments/examples in the schema document.
11. Produce the final reproducible map procedure in `Docs/Reference/Role-Battle-Vertical-Slice-Test-Procedure.md`, covering Aura and BungeeMan combat, civilian work/flee/death/refill, battle phases, merchant interaction/purchase, owner privacy, save/load, late join, and reconnect.
12. Record intentional limitations and follow-up work: ammo/reload, reputation/crime, richer businesses, schedules, role hot-swapping, offline timer progression, and crowd optimization. A known security, data-loss, duplication, or required-test failure is not an acceptable “limitation.”
13. Update the master plan from its current planning status to the actual implemented status, update the implementation index through Day 20, update `Docs/README.md`, and record newly discovered migration work in the TODO tracker.

Do not begin Phase B while any Phase A audit finding remains unresolved or undocumented as an approved non-blocking limitation.

Every Phase B command and packaged topology runner is bounded and returns nonzero on any assertion, build/process, crash, timeout, missing-artifact, staging, or authentication failure. Retain per-process logs under `Saved/Logs/Day20-{Editor|Game|Server|Client}*.log`, machine-readable summaries under `Saved/Reports/Day20-{PhaseA|PhaseB}.json`, and the dated final report below.

## Phase B - Full release verification

1. From a clean build environment, rebuild:
   - `AuraEditor Win64 Development`.
   - `Aura Win64 Development` and `Aura Win64 Shipping`.
   - `AuraServer Win64 Development` and `AuraServer Win64 Shipping`.
2. Build/package the enabled runtime plugins against the same engine, including AuraAbilityGraph, AuraAutoTest, and BehaviorU. Treat missing module, load-phase, dependency, or compile errors as failures.
3. Run every focused Day 01-19 native test namespace, then the complete `Aura` native automation namespace. A log-only check is not a substitute for an assertion and exit code.
4. Run the full AuraAbilityGraph/plugin automation and the project AutoTest suite, including the final vertical-slice XML fixture.
5. Rerun every checked-in Day 01-19 smoke/runner required by its daily gate, in every required listen/dedicated mode, against the post-cleanup Development build. These instrumented runs own the native automation/probe assertions; prior-day results from a pre-cleanup revision cannot be reused.
6. Cook and package both game/client and dedicated-server outputs in the release configuration. Include the final test map in `MapsToCook`.
7. In `Config/DefaultGame.ini`, verify both non-asset directories are staged as UFS:
   - `Content/Config` via `DirectoriesToAlwaysStageAsUFS=(Path="Config")`.
   - `Content/AbilityDefinitions` via `DirectoriesToAlwaysStageAsUFS=(Path="AbilityDefinitions")`.
8. Inspect the staged/package manifest and assert every shipped JSON and required ability-definition XML is present with the expected relative path and case. Also assert test-only saves, credentials, development reports, and grant commands are absent from Shipping.
9. Verify the configured production Online Subsystem authenticates two release test accounts into distinct stable profile IDs; a missing ID, `OnlineSubsystemNull`, fixture identity, display name, or URL option must not open a persistent profile in Shipping.
10. Launch the packaged client and server, validate the economy/role/ability registries from packaged files, and fail on missing, malformed, version-mismatched, or partially loaded definitions.
11. Run the Day 19 cross-process matrix against packaged binaries. Development packaged binaries may use the compile-gated automation probes and fixture identities; Shipping server/listen binaries must be driven through the same public network/RPC surface by a separate Development QA client or external black-box harness, with production-authenticated release accounts. Shipping clients also run launch/staging/authentication sanity without any test endpoint. No probe, grant, fixture identity, or test-only RPC is compiled into Shipping.
   - Packaged listen host plus two remote clients (Development instrumented and Shipping server/client combinations as applicable).
   - Packaged dedicated server plus two remote clients (Development instrumented and Shipping server/client combinations as applicable).
12. Repeat security, owner-privacy, late-join, reconnect, simultaneous death/purchase, replay, persistence, and performance assertions. Editor-only success does not satisfy this gate.
13. Run a legacy version-zero save migration in the packaged build and verify two-player save isolation plus world/population/merchant-stock restoration.
14. Review all build, cook, package, server, client, automation, and performance logs for fatal errors, ensures, missing assets/config, rejected serialization, network failures, and unexpected warnings.

## Final report and evidence

Create `Docs/Reports/Role-Battle-Vertical-Slice-{completion-date}.md` containing:

- Git revision, dirty-worktree state, Unreal version, platform, build configurations, and reference hardware.
- Every build/test/cook/package command and exit code.
- Native and plugin test counts.
- Listen/dedicated topology and client identities used.
- Links/paths to server/client logs, JSON reports, packaged manifests, save-migration evidence, and performance captures.
- Acceptance-matrix results with no silently skipped row.
- Known non-blocking limitations and the follow-up backlog.

## Final definition of done

- Aura and BungeeMan remain stable data-driven combat roles with no duplicate grants.
- Civilian is an independent non-combat AI actor with authoritative population lifecycle.
- Shared faction/battle-zone rules control every intended damage path and death is idempotent.
- Only specifically bound population members become merchants.
- Wallet/inventory are PlayerState-owned, owner-only, authoritative, and isolated per validated persistent profile.
- Merchant transactions are immutable-price, eligibility-checked, replay-safe, atomic, and persist per-instance stock.
- Existing player/enemy/save behavior is not regressed and legacy saves migrate once.
- Full native/plugin automation, cook, packaged config/XML loading, and both packaged network matrices pass.
- Performance and bounded-state thresholds pass.
- Master/index/README/reference/report status matches the delivered implementation.

## Completion gate

The slice is complete only when cleanup precedes and survives the full release pipeline, all required commands and artifacts are recorded in the dated report, both packaged listen and dedicated matrices pass with two remote clients, and no required test is skipped. Any failed build, cook, package, staged-data, migration, security, privacy, persistence, or performance gate blocks release.
