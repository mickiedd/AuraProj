# AuraAbilityGraph and Buff Blueprint-Decoupling Migration Plan

> Status: **In progress — native projectile/pickup runtime implemented; legacy ability cleanup remains**
> Prepared: 2026-08-02  
> Audience: a new implementation agent  
> Read first: `Buff-DataDriven-Migration-Plan.md`, `GAS-DataDriven-Rewrite-Plan.md`, and `GAS-Migration-TODOs.md`

## Objective

Remove the remaining dependency on Blueprint-authored **gameplay behavior and gameplay-definition UAssets** from the active AuraAbilityGraph player path and the data-driven buff/pickup path.

The target runtime uses C++ for behavior, XML/JSON for configuration, native `/Script/...` classes in active definitions, and UAssets only for presentation such as meshes, materials, animations, Niagara/particles, sounds, icons, and Gameplay Cues.

This is not an asset-free migration. It is complete when gameplay behavior and values can be changed without editing a Blueprint or GameplayEffect asset. Presentation UAssets are allowed and expected.

## Verified Baseline

As of 2026-08-02:

- Buff values and policies live in `Content/Config/GameplayEffects.json`.
- `AAuraEffectActor` creates native pickup GameplayEffects. The six legacy buff GE assets and legacy effect-class properties are gone.
- Active player abilities are selected in `Content/Config/RoleConfig.json` and loaded from XML.
- `FireBolt.xml` still loads `BP_FireBolt_C`.
- `FireBlast.xml` still loads `BP_FireBall_C`.
- Buff actors still use `BP_HealthPotion`, `BP_ManaPotion`, `BP_HealthCrystal`, `BP_ManaCrystal`, `BP_FireArea`, and `BP_TestActor` for components/presentation/overlap wiring.
- Legacy player `GA_*`, `GE_Cost_*`, and `GE_Cooldown_*` assets remain. `BP_AuraCharacter` contains a `GA_FireBolt` reference, and `DA_AbilityInfo` references legacy abilities.
- Enemy and passive abilities still use legacy GA/GE assets. Attribute initialization and legacy enemy damage GEs are separate systems and must not be deleted casually.

Unreal Asset Registry/reference data and a successful cook are authoritative. Text or binary search alone cannot prove a UAsset is unreferenced.

## Scope

Required:

1. Replace `BP_FireBolt` and `BP_FireBall` with native projectiles plus data configuration.
2. Move Blueprint-owned projectile components, defaults, collision, movement, FX/audio references, and FireBall outbound/return behavior into C++ and configuration.
3. Replace the six buff actor Blueprints with native configurable actors and replace every placed/spawned instance.
4. Remove dead legacy player GA/cost/cooldown assets only after proving they have no remaining consumers.
5. Add assertion-based automation, multiplayer/runtime smoke tests, and cook validation.
6. Build, debug, inspect complete logs, fix issues, and repeat until clean.
7. Update all related documentation and the final retained/deleted asset inventory.

Non-goals:

- Do not convert presentation UAssets solely because they are UAssets.
- Do not delete enemy, passive, attribute-initialization, or shared damage assets unless those owning paths are migrated and tested.
- Do not change balance, projectile timing/collision, pickup behavior, or network authority unintentionally.

## Target Architecture

```text
RoleConfig.json -> AbilityDefinitions/*.xml -> C++ graph nodes
    -> native projectile class -> ProjectileDefinitions.json
        -> soft presentation asset references

map/spawner -> native configured pickup actor -> PickupDefinitions.json
    -> effect name in GameplayEffects.json -> native pickup GameplayEffect
    -> soft presentation asset references
```

Parse and validate configuration once, cache immutable definitions, and produce errors containing the definition name and source file. Do not reparse a complete JSON file for every spawn or overlap.

## Phase 0: Baseline and Dependency Manifest

Before edits:

1. Read repository `AGENTS.md` instructions and record `git status --short`; preserve unrelated user changes.
2. Build `AuraEditor Win64 Development`.
3. Run `Aura.Buffs` and the complete AuraAbilityGraph smoke suite. Save baseline logs and distinguish existing unrelated warnings.
4. Inventory all hard, soft, string, map, config, default-object, and data-asset references to the two projectile and six pickup Blueprints.
5. Use an editor Asset Registry commandlet/script or Reference Viewer data to write a machine-readable referencer/dependency report under `Saved/`.
6. Record every map and spawner containing pickup instances.
7. Snapshot/export relevant Blueprint graphs, components, defaults, collision, movement, and asset references. Snapshots are migration aids; remove stale snapshots when their packages are deleted.

Do not delete assets if the baseline fails or the inventory is incomplete.

## Phase 1: Native Data-Configured Projectiles

### 1.1 Projectile definitions

Add `Content/Config/ProjectileDefinitions.json` (or a consistently named equivalent). A definition must be able to reproduce settings currently baked into `BP_FireBolt` and `BP_FireBall`:

```json
{
  "projectiles": {
    "fireBolt": {
      "nativeClass": "/Script/Aura.AuraProjectile",
      "collisionRadius": 15.0,
      "initialSpeed": 550.0,
      "maxSpeed": 550.0,
      "gravityScale": 0.0,
      "lifeSpan": 15.0,
      "worldStaticResponse": "Block",
      "mesh": "/Game/...",
      "meshScale": [0.3, 0.3, 0.3],
      "flightTrail": "/Game/...",
      "impactEffect": "/Game/...",
      "impactSound": "/Game/...",
      "loopingSound": "/Game/..."
    }
  }
}
```

Add FireBall fields for outbound duration/distance or speed, return behavior, collision policy, and presentation. Prefer a deterministic native movement state machine over a Blueprint Timeline. If a curve is necessary to preserve behavior, load it through a documented soft reference.

Validation must reject missing definitions, invalid native classes, Blueprint-generated classes for migrated definitions, bad numeric ranges, unsupported collision modes, and missing asset paths. Ensure JSON-only soft assets are explicitly included in cooking when ordinary dependency discovery cannot find them.

### 1.2 Ability node contract

Update both `SpawnProjectile` and `SpawnProjectiles` to accept a named definition, preferably:

```xml
<property name="ProjectileDefinition" value="fireBolt"/>
```

A temporary `ProjectileClass` compatibility path is acceptable during migration, but final active XML must resolve to native `/Script/Aura...` classes. Share loading/configuration code between the two nodes.

### 1.3 FireBolt

Move all required BP behavior/defaults into `AAuraProjectile` or a focused native subclass:

- components and attachment;
- collision object type/responses and enable timing;
- movement, homing, mesh/scale, trails, looping sound and impacts;
- overlap and blocking-hit paths;
- friendly/source filtering and exactly-once damage;
- lifespan, cleanup, authority, and replication.

Remove logs/comments that only work around BP-baked defaults once the BP is not loaded, while retaining useful structured diagnostics.

### 1.4 FireBall

`AAuraFireBall::StartOutgoingTimeline` is a required `BlueprintImplementableEvent`. Replace it with native behavior and remove that dependency. Preserve outgoing travel, return to `ReturnToActor`, late `OnRep_ReturnToActor`, pass-through-wall behavior, damage, final Gameplay Cue, and safe termination when the return actor becomes invalid.

### 1.5 Switch active XML

Update `FireBolt.xml` and `FireBlast.xml`. Neither may contain a `BP_FireBolt_C` or `BP_FireBall_C` path. Validate through the actual game/import path, not only a generic XML parser.

Phase 1 acceptance:

- Both abilities spawn native classes without loading either projectile BP.
- Behavior and presentation match the recorded baseline.
- Bad definitions fail safely with a single actionable error.
- Assertion-based tests cover parsing, collision, damage, return, cleanup, authority, and replication.
- A listen-server/client test proves server-authoritative damage without duplicate client application.

## Phase 2: Native Data-Configured Pickups

### 2.1 Pickup definitions

Add `Content/Config/PickupDefinitions.json`. `GameplayEffects.json` remains responsible for effect behavior; this file controls actor interaction and presentation.

Each definition should contain:

- native actor class;
- effect names and application/removal policies;
- destroy-on-application, apply-to-enemies, and actor level;
- collision shape/size/object/responses;
- mesh/material and transform soft paths;
- rotation and sinusoidal movement settings;
- optional VFX/audio;
- respawn/lifetime only where existing behavior needs it.

Validate cross-file effect names before overlap.

### 2.2 Self-contained native actor

Extend `AAuraEffectActor` or add `AAuraConfiguredEffectActor`. It must create/configure the root, collision, mesh, and optional presentation components in C++. It must bind overlap/end-overlap delegates natively and expose a stable definition name for placed and spawned instances.

Blueprint events must not be required to call `OnOverlap`, `OnEndOverlap`, movement initialization, effect application, or destruction. Reuse the existing validated effect path rather than duplicating `GameplayEffects.json` parsing.

### 2.3 Replace placed/spawned instances

Create an editor migration utility or commandlet that scans every project map/spawner, replaces old classes, preserves transform/folder/data-layer/level/tags/instance overrides, saves and reloads maps, and emits a replacement/error report.

| Old Blueprint | New definition |
|---|---|
| `BP_HealthPotion` | `healthPotion` |
| `BP_ManaPotion` | `manaPotion` |
| `BP_HealthCrystal` | `healthCrystal` |
| `BP_ManaCrystal` | `manaCrystal` |
| `BP_FireArea` | `fireArea` |
| `BP_TestActor` | `testAttribute` or a documented replacement |

Account for duplicate historical names/locations rather than assuming filename uniqueness.

Phase 2 acceptance:

- All six behaviors work after closing and reopening the editor.
- No map, spawner, config, or test references a migrated BP class.
- Tests assert instant, periodic, delayed-periodic, infinite/removal, destroy, filtering, repeated overlap, invalid target, and end-play cleanup behavior.
- Presentation/collision work in a cooked build, not just PIE.

## Phase 3: Legacy Player Ability Cleanup

### 3.1 Classify candidates

Create a reviewed deletion manifest:

| Asset | Referencers before | Replacement | Referencers after | Decision |
|---|---:|---|---:|---|

Classify every candidate as safe now, retained for enemies, retained for passives, retained for initialization/damage, retained for UI/metadata, or blocked/unknown.

### 3.2 Remove stale references

- Replace/clear the `GA_FireBolt` reference in `BP_AuraCharacter`, then compile and resave it.
- Establish whether `DA_AbilityInfo` is still a runtime source. If `AbilityInfo.json` replaces it, migrate every consumer first; otherwise retain and document it as presentation metadata.
- Inspect role defaults, save restoration, spell menus, input binding, ability granting, tests, and fallback paths.
- Fix redirectors only in the narrow affected directories and rerun the Asset Registry report.

### 3.3 Gated deletion

Expected review candidates include legacy player `GA_FireBolt`, `GA_FireGun`, `GA_FireBlast`, `GA_ArcaneShards`, `GA_Electrocute`, and their player cost/cooldown GEs. This is not pre-authorization to delete them.

Do not delete enemy, passive, attribute-init, or shared damage assets while another runtime path uses them. Delete confirmed-dead `.snapshot.json` files with their packages.

Phase 3 acceptance:

- Active player startup/LMB grants only data-driven definitions/native runtime classes.
- Every deleted asset has zero Asset Registry, map, config, string, hard, and soft referencers.
- Editor startup, map load, PIE, standalone, and cook show no missing packages.
- Config failure cannot silently fall back to a legacy player GA.

## Phase 4: Wider GAS Boundary

This phase is separate but required before claiming **all Aura gameplay** is Blueprint-independent.

### Enemies

Port and test `GA_EnemyFireBolt`, `GA_RangedAttack`, `GA_MeleeAttack`, and `GA_HitReact` as tracked in `GAS-Migration-TODOs.md`. Verify AI integration, grants, targeting, montage events, damage/cooldown, death, and authority before deleting their assets.

### Passives

Choose per passive: native infinite GE for simple state, native component/listener for event logic, or an AuraAbilityGraph passive definition only when an action graph fits. Migrate Halo of Protection, Life Siphon, Mana Siphon, and event-listener startup behavior before deletion.

Phase 4 is optional for the narrow claim “active player AuraAbilityGraph and buff systems are Blueprint-independent,” but mandatory for the project-wide claim.

## Required Automated Tests

Tests must assert and fail Unreal Automation; log-only smoke checks are insufficient.

Configuration:

- shipped definitions parse and cross-references resolve;
- malformed JSON, unknown definitions, bad ranges/enums/classes/assets fail predictably;
- Blueprint projectile classes are rejected on migrated paths;
- caching prevents per-spawn/per-overlap reparsing and can reset in tests.

Projectiles:

- native class/config/components/defaults;
- hostile damage once; source/friendly ignored;
- wall impact/destruction and lifespan/audio cleanup;
- homing/non-homing/multi-projectile XML behavior;
- FireBall outbound/return/replication/world collision/damage/termination;
- server-only damage application.

Pickups:

- all shipped definition/effect mappings;
- expected deltas for instant/duration/periodic/delayed/infinite effects;
- no missing SetByCaller errors;
- removal, destruction, filtering, repeated overlap, invalid target, and cleanup;
- native delegate binding without Blueprint events.

References/cook:

- active XML has no generated projectile class;
- active role path has no legacy player GA;
- migrated pickup BP paths are absent from maps/config;
- Asset Registry has zero referencers before deletion;
- cooked validation proves JSON-only presentation assets are packaged.

## Build, Debug, Smoke-Test, and Log Loop

Build:

```powershell
& 'C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat' AuraEditor Win64 Development '-Project=C:\Git\AuraProj\Aura.uproject' -WaitMutex -NoHotReloadFromIDE
```

Focused automation (run suites separately if this UE version does not chain commands reliably):

```powershell
& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -unattended -nop4 -nullrhi -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests Aura.Buffs' '-TestExit=Automation Test Queue Empty' -log

& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -unattended -nop4 -nullrhi -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests Aura.Projectiles' '-TestExit=Automation Test Queue Empty' -log

& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi -DisablePlugins=RiderLink -log
```

Runtime scenarios:

1. FireBolt against enemy, friend, and wall.
2. FireBlast outbound/return with multiple targets.
3. All six pickups/areas.
4. Listen server plus one client; verify exactly-once effects.
5. Reload/transition a map containing migrated actors.
6. Repeat a minimal scenario in a packaged build.

After every run, inspect the complete new log. Treat newly introduced errors/fatals, failed assertions, ensures, Accessed None, missing objects/classes/packages, config parse failures, missing SetByCaller values, invalid tags, BP compile warnings, duplicate effects, replication warnings, and cook-exclusion warnings as failures.

Maintain this table in the implementation report:

| Iteration | Failure/signature | Root cause | Fix | Rerun result |
|---|---|---|---|---|

Run a Win64 Development cook/package for relevant maps. This gate is mandatory because assets referenced only as JSON strings can work in editor but be absent from packaged builds. Launch it and verify projectile/pickup behavior and logs.

Final hygiene: validate JSON/XML, check JavaScript if editors changed, run `git diff --check`, review `git status --short`, search for deleted names, reload/resave changed packages, and rerun Asset Registry validation.

## Deletion Gates

Delete an asset only when:

1. behavior/defaults have a native/config replacement;
2. the replacement has assertion-based tests;
3. maps/spawners are migrated and reloaded;
4. Asset Registry reports no outside referencers;
5. code/config/XML/string search is clean;
6. narrow-directory redirectors are fixed;
7. editor/map/PIE logs have no missing objects;
8. cook and packaged smoke tests pass.

Delete in small batches: projectile assets, pickup assets, then legacy player GA/GEs. Rebuild and rerun relevant tests after each batch.

## Definition of Done

Narrow migration:

- FireBolt and FireBlast use native projectile definitions; both projectile BPs are deleted and unreferenced.
- FireBall has no Blueprint event required for gameplay.
- All six pickup BPs are replaced everywhere, deleted, and unreferenced.
- Active player abilities use no legacy player GA/cost/cooldown assets.
- Presentation UAssets are documented as allowed dependencies.
- focused automation, the entire AuraAbilityGraph suite, multiplayer smoke, build, cook, packaged launch, and log analysis pass cleanly.
- `Buff-DataDriven-Migration-Plan.md`, `GAS-Abilities-Documentation.md`, and `GAS-Migration-TODOs.md` contain the final architecture, results, retained assets, and deletions.

Wider migration: enemy and passive Phase 4 work must pass equivalent gates before claiming all gameplay logic is Blueprint-independent.

## Agent Handoff Checklist

- [ ] Read this plan and predecessor docs; inspect `AGENTS.md` and preserve unrelated changes.
- [ ] Capture baseline build/tests/logs/references/maps/BP defaults.
- [ ] Implement cached, validated projectile config and native FireBolt/FireBall.
- [ ] Switch XML and test multiplayer behavior.
- [ ] Implement cached, validated pickup config and native actor components/delegates.
- [ ] Replace/reload all placed and spawned pickup actors.
- [ ] Produce and review the deletion manifest.
- [ ] Delete assets only in gated batches.
- [ ] Add assertion-based configuration/projectile/pickup/reference tests.
- [ ] Build, debug, smoke-test, inspect logs, cook/package, and iterate to a clean result.
- [ ] Update documentation with commands, test counts, retained presentation assets, deletions, and deferred work.
