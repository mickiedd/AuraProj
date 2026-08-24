# Day 12 - Add the Battle Director

## Goal

Add one server-spawned, replicated, always-relevant `AAuraBattleDirector : AInfo` that owns battle phase, validated zone policy, safe-zone precedence, and battle event identity. The final server damage boundary resolves zone context from the target’s current authoritative location; caller-supplied zone/event context is never trusted.

## BungeeMan Gun Skill checkpoint

Resolve FireGun damage against the authoritative target location, battle phase, safe-zone, and civilian-casualty policy. BungeeMan-versus-Enemy behavior must remain intact; BungeeMan-versus-Civilian becomes legal only through the explicit Day 12 zone-policy opt-in, never through caller-supplied FireGun context.

## Prerequisite gate

- Day 11 provides exactly-once neutral `FAuraDeathEvent` publication through the one GameMode-owned dispatcher and serialized source/ability/damage attribution.
- Day 10 has explicit work/observation/shelter marker IDs and zone IDs.
- Day 09 populations carry stable map and zone IDs.
- Day 04 still has one final server damage-application boundary used by projectile, beam, radial, hitscan, and melee paths.

## New files

- `Source/Aura/Public/Battle/AuraBattleDirector.h`
- `Source/Aura/Private/Battle/AuraBattleDirector.cpp`
- `Source/Aura/Public/Battle/AuraBattleZoneTypes.h`
- `Source/Aura/Public/Battle/AuraBattleZoneConfig.h`
- `Source/Aura/Private/Battle/AuraBattleZoneConfig.cpp`
- `Content/Config/BattleZones.json`
- `RunRoleBattleDay12NetworkSmoke.ps1`

## Files to modify

- `Source/Aura/Public/Game/AuraGameModeBase.h`
- `Source/Aura/Private/Game/AuraGameModeBase.cpp`
- `Source/Aura/Public/Combat/AuraCombatTypes.h`
- `Source/Aura/Public/Combat/AuraCombatRules.h`
- `Source/Aura/Private/Combat/AuraCombatRules.cpp`
- `Source/Aura/Public/Combat/AuraCombatStateComponent.h`
- `Source/Aura/Private/Combat/AuraCombatStateComponent.cpp`
- `Source/Aura/Public/AuraAbilityTypes.h`
- `Source/Aura/Private/AuraAbilityTypes.cpp`
- `Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h`
- `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp`
- `Source/Aura/Public/AbilitySystem/AuraAttributeSet.h`
- `Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp`
- `Source/Aura/Public/World/AuraPopulationManager.h`
- `Source/Aura/Private/World/AuraPopulationManager.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Maps/RoleBattleCivilianTest.umap`

All graph/projectile damage producers are regression-tested but do not receive per-producer zone special cases. The Day 04 final boundary remains the only integration point.

## Director ownership and replication contract

- `AAuraBattleDirector` derives from `AInfo`.
- Its constructor sets `bReplicates=true`, `bAlwaysRelevant=true`, `bNetLoadOnClient=true`, disables movement replication, and uses an appropriate net update frequency for phase changes.
- `AAuraGameModeBase` spawns exactly one director with `SpawnActorDeferred` on authority. Clients never spawn it and never own it.
- GameMode keeps the authoritative pointer. `AAuraBattleDirector::Get(const UObject*)` finds the replicated actor on clients without attempting to access GameMode.
- Duplicate director spawn is a fatal configuration error for the Day 12 gate.
- Replicated state includes `CurrentPhase`, `ActiveBattleEventId`, configuration version/hash, and the first-slice zone runtime states. Use `ReplicatedUsing` callbacks; use a Fast Array if per-zone runtime state can change independently.
- Static zone policy is loaded/validated on authority. Replicate every zone summary needed by client UI/debug so a late client does not depend on reading a server-only object.
- `OnRep_CurrentPhase` and zone-state OnRep delegates are cosmetic/informational. They cannot authorize damage.

## Revised GameMode startup ordering

Day 12 updates server startup to:

1. During `InitGame`, complete RoleConfig publication, construct the Day 09 population manager, load/validate population and work definitions without spawning, and create the Day 11 death dispatcher.
2. Enter `StartPlay` with those services ready. Spawn the one deferred `AAuraBattleDirector` and parse `BattleZones.json` into an unpublished candidate; do not cross-validate references yet.
3. Call `Super::StartPlay` so placed spawn volumes and Day 10 activity markers can register with the already-existing manager.
4. Reconcile registrations with an idempotent placed-actor discovery pass, then jointly cross-validate the zone candidate against loaded population/work rows plus discovered volume/marker IDs.
5. Only after joint validation succeeds, finish spawning/configuring the director, bind it once to `UAuraDeathPolicyDispatcher::OnAuthoritativeDeath`, and publish its initial replicated Peace state.
6. Release Day 09's held next-tick `FinalizeInitialPopulation`; spawn only after director/zone state is ready.
7. Accept gameplay damage only after the dispatcher, director, zone policy, population definitions, registrations, and initial population all report the same ready generation.

Any failure leaves the startup coordinator closed and spawns no partial population. This ordering avoids Actor BeginPlay races and ensures Civilian AI/spawning see zone data from their first gameplay frame.

Closed-startup handling is explicit: a failed initial candidate sets the server readiness state to unhealthy and rejects persistent/interactive login with an actionable reason (or terminates startup when the deployment is configured for fail-fast operation). It must never advertise a ready world with a closed coordinator. There is no permissive default-zone fallback. Last-known-good retention is allowed only for a previously published reload candidate; an initial failure remains not-ready.

## Versioned battle-zone data contract

`BattleZones.json` contains `schemaVersion` and a `zones` array. Each zone defines:

- Unique `zoneId`.
- `mapId`.
- Axis-aligned bounds for the first slice, or an explicitly referenced placed volume ID.
- Integer `priority`.
- `bSafeZone`.
- Per-phase faction relationship overrides.
- Civilian protection/casualty policy.
- Population IDs.
- Work/observation/shelter marker IDs.
- Enabled phase set.

Validation rejects unsupported versions, duplicate IDs, non-finite/inverted bounds, unknown phases/factions, missing population/marker references, invalid relationship rows, and duplicate placed-volume bindings.

Overlapping non-safe zones with equal priority are allowed only because the deterministic tie rule below is mandatory; validation logs the overlap and tie.

## Authoritative zone-resolution rules

Extend the Day 03 `FAuraCombatRuleContext` with the director’s immutable authoritative policy snapshot and server-known damage metadata. Public client/request APIs do not accept an authoritative zone ID, battle event ID, permission Boolean, policy snapshot, or relationship result.

At the final server damage boundary:

1. Discard/overwrite any zone or event fields present in client target data or incoming `FDamageEffectParams`.
2. Read the target avatar’s current authoritative world location immediately before applying the Gameplay Effect.
3. Resolve all enabled zones containing that target location.
4. If any matching zone has `bSafeZone=true`, discard every non-safe candidate. Safe protection always wins over every conflict zone regardless of priority.
5. Within the remaining candidate set (safe-only when any safe zone matched, otherwise all matching non-safe zones), choose the highest integer priority.
6. If multiple remaining zones share the highest priority, choose the lexicographically smallest canonical `zoneId`. This rule deterministically chooses the diagnostic/attribution zone even when every safe policy protects identically.
7. If no zone matches, use the explicit default relationship policy.
8. Evaluate phase, identities, life state, targetability, and the chosen zone policy in `AuraCombatRules`.
9. On acceptance, stamp the server’s chosen `BattleZoneId` and `ActiveBattleEventId` into the outgoing effect context.
10. Rejection returns a structured reason including phase and resolved zone, but creates no damage Gameplay Effect.

Target location controls Civilian protection for the first slice. Source location may be reported for diagnostics but cannot weaken target safe-zone protection.

Projectiles and delayed effects re-resolve at impact/application time; they may not carry a permission decision from launch time.

Client calls to `AuraCombatRules` are preview-only and must be labeled non-authoritative. The server repeats the complete resolution.

## Effect-context and death-event integration

Use the optional fields already declared and serialized on Day 04 in `FDamageEffectParams`/`FAuraGameplayEffectContext`:

- `FName BattleEventId`
- `FName BattleZoneId`

Day 12 activates authority-only stamping and regression-verifies, rather than redeclares:

- Existing getters/setters and `Duplicate` in `Source/Aura/Public/AuraAbilityTypes.h`.
- Existing `NetSerialize` presence bits and read/write/reset behavior in `Source/Aura/Private/AuraAbilityTypes.cpp`; do not allocate duplicate bits.
- `UAuraAbilitySystemLibrary` context accessors and final application code.
- `UAuraAttributeSet` fatal-context extraction.
- `FAuraDeathEvent` construction in `UAuraCombatStateComponent`.

Only the server-resolved final boundary writes these fields. The Day 11 neutral death event copies them from the accepted damage context, allowing the director to subscribe and record casualties without changing exactly-once death ownership.

An event ID is generated on the server when leaving Peace for Alert, or on a direct Peace→Conflict transition. It remains unchanged through Alert, Conflict, and Cleanup, clears only on Cleanup→Peace, and every later event receives a distinct ID. It is not a client-generated GUID.

## Phase and notification contract

Supported phases:

- Peace
- Alert
- Conflict
- Cleanup

Only authority may call phase-transition functions. Each valid transition:

- Validates the transition table.
- Updates phase and, when required, event ID once.
- Re-evaluates zone runtime state.
- Broadcasts a server phase delegate to AI/population systems.
- Replicates state for UI and late joiners.

The director binds once to `UAuraDeathPolicyDispatcher::OnAuthoritativeDeath` after successful startup and unbinds during EndPlay. It records battle casualties only when the event's server-stamped event/zone IDs match an active event. It does not own death rewards or population refill.

## Implementation steps

1. Add the explicit replicated `AInfo` director and safe server spawn/discovery path.
2. Add versioned config parsing and cross-validation.
3. Implement exact target-location, safe-candidate precedence, then priority and lexical tie resolution within the chosen candidate set.
4. Make the final server damage boundary discard caller context and resolve immediately before application.
5. Activate server stamping of the Day 04 battle ID fields and verify existing effect-context duplication/serialization/reset behavior, AttributeSet extraction, and neutral death-event copying.
6. Add authority-only phase transitions and replicated OnRep delegates.
7. Subscribe to neutral death events without moving death-policy ownership.
8. Reorder GameMode startup so the director is ready before population spawning.
9. Add structured rejection/debug logs with source, target, phase, candidate zones, chosen zone, policy, and event ID.
10. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day12.BattleZoneSchemaValidation`
- `Aura.RoleBattle.Day12.SafeZonePrecedence`
- `Aura.RoleBattle.Day12.OverlappingSafeZonePriorityAndLexicalTie`
- `Aura.RoleBattle.Day12.PriorityAndLexicalTie`
- `Aura.RoleBattle.Day12.TargetLocationResolution`
- `Aura.RoleBattle.Day12.UntrustedCallerContextOverwritten`
- `Aura.RoleBattle.Day12.ProjectileBoundaryReevaluation`
- `Aura.RoleBattle.Day12.BattleAttributionNetSerialize`
- `Aura.RoleBattle.Day12.DeathEventCarriesBattleIds`
- `Aura.RoleBattle.Day12.ValidPhaseTransitions`
- `Aura.RoleBattle.Day12.EventIdLifecycle`
- `Aura.RoleBattle.Day12.InitialFailurePublishesUnhealthy`

## Current executable baseline smoke

`RunRoleBattleDay12NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. The 2026-08-24 baseline passes both modes and records eight assertions: static contracts, coordinated world readiness after joint registration validation, exactly one authority director, forged context overwrite, Peace/Conflict policy behavior and event lifecycle, director replication on both clients, all three population states on both clients including late join, and no crash signature.

This baseline is executable director/readiness evidence, not the full Day 12 completion gate.

## Remaining completion acceptance matrix

Before Day 12 is marked complete, extend the runner or a successor to:

1. Verifies exactly one server-spawned always-relevant director and no client-owned duplicate.
2. Compares phase, event ID, zone summaries, version/hash, and OnRep notifications on two clients.
3. Tests Peace protection, allowed Conflict casualty, outside-zone default policy, overlapping-zone priority, lexical tie, and safe-zone precedence.
4. Fires a delayed projectile while the target moves between conflict and safe zones; impact-time target location must decide.
5. Sends forged zone/event IDs from a remote client and proves the server overwrites them.
6. Kills one allowed Civilian and verifies its neutral death event carries the server-chosen zone/event IDs exactly once.
7. Connects a late client during Conflict and verifies complete director state and UI/debug zone result.
8. Confirms every damage producer still reaches the same final rule boundary.

The current baseline runner enforces bounded startup/readiness, phase, assertion, late-join, and teardown timeouts; returns nonzero on any implemented assertion, child-process, crash, timeout, duplicate director, or missing artifact; and stops only processes it created. The completion extension must retain those guarantees while adding the delayed-projectile, allowed-casualty, and complete damage-producer matrix. Reports are written to `Saved/Reports/Day12-{Listen|Dedicated}.json` beside isolated process logs.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day12; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day12Automation.log'

& '.\RunRoleBattleDay12NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay12NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

One server-spawned replicated `AAuraBattleDirector : AInfo` authoritatively owns phase, zones, and battle event identity. Startup cross-validates only after population definitions and placed registrations exist. Final damage permission always uses the target's current server location, filters to safe candidates when present, then applies priority and lexical tie-break within that set; caller context is untrusted. Event IDs span Alert through Cleanup, accepted damage and exactly-once death events carry the already-serialized server zone/event fields, and existing plus late clients receive identical director state. All build, native, listen, and dedicated gates pass.
