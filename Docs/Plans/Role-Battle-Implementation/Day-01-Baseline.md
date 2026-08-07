# Day 01 - Establish the Baseline

## Goal

Record the current behavior and create a reproducible starting point before changing role, combat, or death code.

## Execution status — 2026-08-07

The baseline checks have been executed against the current checkout. Results, commands, fixture details, combat-path inventory, and remaining blockers are recorded in [Docs/Reports/Role-Battle-Baseline-2026-08-07.md](../../Reports/Role-Battle-Baseline-2026-08-07.md). The follow-up pass fixed the persistent-ASC respawn attribute accumulation and the unassigned SetByCaller magnitude errors. The checked-in Day 1 smoke runner now passes Login/BungeeMan asset wiring, both live damage directions, and two runtime respawns with stable vitals. Only rendered/editor visual confirmation remains open.

## Read first

- Docs/Plans/Role-Creation-and-Battle-System-Plan.md
- Docs/Tracking/GAS-Migration-TODOs.md
- Docs/Reference/GAS-Abilities-Documentation.md

## Files to inspect

- Content/Config/RoleConfig.json
- Config/DefaultEngine.ini
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp
- Source/Aura/Private/Actor/AuraProjectile.cpp
- Source/Aura/Private/Actor/AuraFireBall.cpp
- Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp
- Source/Aura/Private/Actor/AuraEffectActor.cpp
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp

## Preconditions

- Record `git status --short`, `git rev-parse HEAD`, the Unreal Engine version, and the configured `UE_ENGINE_ROOT`. Do not clean or overwrite unrelated work.
- Verify the configured map and all role/ability assets before starting gameplay tests. `Config/DefaultEngine.ini` currently points to `/Game/Maps/Login.Login`; if that map is unavailable, record it as a pre-existing blocker and identify an existing or dedicated baseline fixture map explicitly. Do not silently substitute a different map.
- Record whether the baseline uses PIE, standalone, or a dedicated server. If a dedicated server is used, record the server map, ports, client command line, and server build result.

## Steps

1. Record the repository state and environment from the Preconditions section. The current worktree already contains documentation changes; list them separately from baseline failures.
2. Build the editor target with `build_test.bat` (`AuraEditor Win64 Development`). If the baseline uses a dedicated server, also run `BuildDedicatedServer.bat` (`AuraServer Win64 Development`) and record any source-built Unreal Engine requirement. If a standalone game target is required, invoke Unreal Build Tool explicitly for `Aura Win64 Development` and record the command.
3. Run the native Unreal automation tests from `Source/Aura/Private/Tests` through the Editor Automation panel, filtering the `Aura` tests. Run the existing AuraAbilityGraph smoke test with `RunSmokeTest.bat` when the plugin is enabled. Record each suite's command, pass/fail result, log path, and failures separately from this task.
4. Prepare the baseline fixture and record its map, actor classes, transforms, controller/possession state, enemy class/level, navmesh state, player start, and any console commands or cheats used. The fixture must contain enough actors to test both damage directions and player respawn. If the current project has no usable map, keep the baseline blocked until the missing-map issue is explicitly resolved.
5. Start a playable test with the Aura role:
   - Confirm body mesh and animation.
   - Confirm staff and FireBolt.
   - Confirm FireBlast, ArcaneShards, and Electrocute.
   - Confirm damage, health, death, and player respawn.
6. Start a playable test with BungeeMan:
   - Confirm rifle mesh.
   - Confirm Muzzle socket.
   - Confirm FireGun projectile, damage, and cooldown.
7. Test existing Player versus Enemy damage and Enemy versus Player damage using the fixture. Record source actor, target actor, ability, authority mode, pre/post health, and whether death/loot/respawn side effects occurred.
8. Run a source inventory with `rg` for `IsNotFriend`, `ActorHasTag`, `GetAllActorsWithTag`, `ApplyDamageEffect`, and `FDamageEffectParams` across `Source` and `Plugins/AuraAbilityGraph`. For every result, record the file, function, path category (projectile, beam, radial, hitscan, melee, AI targeting, pickup/effect filtering), whether the relationship check is direct or indirect, and whether it runs on the server.
9. Record the exact map and fixture manifest used for the baseline, including asset paths, actor names/classes, transforms, role configuration, test mode, engine version, and log locations.

## Deliverables

- Baseline test notes.
- A list of pre-existing failures.
- A complete combat-path inventory: every direct `IsNotFriend` or actor-tag check, every `ApplyDamageEffect` producer, the relationship-check location, and the authority boundary.
- A repeatable map/setup manifest for later regression tests, or an explicit missing-map blocker with the exact configured and available map paths.
- Test commands, environment details, and log paths sufficient for another developer to reproduce the baseline.

## Completion gate

Do not continue until the map/setup is reproducible and Aura, BungeeMan, existing Enemy combat, player respawn, and the available automation suites are either working or their failures are explicitly recorded. This prevents later changes from hiding unrelated regressions.
