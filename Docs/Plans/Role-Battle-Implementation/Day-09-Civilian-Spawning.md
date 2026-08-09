# Day 09 - Add Civilian Population Spawning

## Goal

Create the server-only `UAuraPopulationManager` now and make it the sole owner of Civilian population definitions, deterministic member slots, initial spawning, and the runtime registry. Day 13 extends this same manager with corpse cleanup and refill; no temporary GameMode registry is allowed.

## BungeeMan Gun Skill checkpoint

Ensure population spawning never inherits a player FireGun ability, rifle, or player role state. Each spawned Civilian must have its stable server-owned identity before any later FireGun target/rule query, and the population manager must not become a second owner of player skill grants.

## Prerequisite gate

- Day 08 can spawn a role-derived `AAuraCivilian` through deferred spawning and set its authority-only requested role ID before `FinishSpawning`; the resulting `FAuraAppliedRoleState`, not the request, is what replicates.
- The Day 08 listen and dedicated late-join gates pass.
- `Content/Config/LevelConfig.json` contains stable level IDs that can be cross-checked against population `mapId` values.

## New files

- `Content/Config/PopulationSpawnTable.json`
- `Content/Config/CivilianWorkProfiles.json`
- `Source/Aura/Public/World/AuraPopulationTypes.h`
- `Source/Aura/Public/World/AuraPopulationSpawnDefinition.h`
- `Source/Aura/Private/World/AuraPopulationSpawnDefinition.cpp`
- `Source/Aura/Public/World/AuraPopulationManager.h`
- `Source/Aura/Private/World/AuraPopulationManager.cpp`
- `Source/Aura/Public/World/AuraCivilianSpawnVolume.h`
- `Source/Aura/Private/World/AuraCivilianSpawnVolume.cpp`
- `Content/Blueprints/World/Civilian/BP_AuraCivilianSpawnVolume.uasset`
- `RunRoleBattleDay9NetworkSmoke.ps1`

## Files to modify

- `Source/Aura/Public/Game/AuraGameModeBase.h`
- `Source/Aura/Private/Game/AuraGameModeBase.cpp`
- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Maps/RoleBattleCivilianTest.umap`

`Content/Config/LevelConfig.json` is read for map-ID validation but is not repurposed as a population registry.

## Ownership and authority contract

- `UAuraPopulationManager` is a transient `UObject` owned and strongly referenced by `AAuraGameModeBase`. Because GameMode exists only on authority, there is no client-side population manager and no client spawn API.
- GameMode exposes a const/debug getter and no Blueprint/client mutation entry point.
- The manager is the only owner of loaded population rows, member slots, actor-to-member mappings, spawn retries, and later lifecycle timers.
- `AAuraCivilianSpawnVolume` only registers a validated volume ID and supplies candidate transforms. It never decides counts or spawns actors itself.
- `AAuraCivilian` has one `ReplicatedUsing` read-only `FAuraPopulationMemberState` assigned only by the manager during deferred spawning. Its exact fields are `FName PopulationId`, `int32 PopulationSlotIndex`, `FName PopulationMemberId`, `FName WorkProfileId`, `FName ZoneId`, and `FName MerchantDefinitionId`. The complete struct is in the initial bunch, and one idempotent OnRep drives UI/debug/late-join consumers. No field is a client setter or a second role identity.
- Day 09 creates no death refill timer. It reserves the slot and records actor destruction for diagnostics; Day 11 records authoritative deaths and Day 13 enables cleanup/refill.

## Versioned population data contract

`PopulationSpawnTable.json` begins with:

```json
{
  "schemaVersion": 1,
  "populations": []
}
```

Each row contains:

- `populationId`: unique stable `FName`.
- `mapId`: required level ID from `LevelConfig.json`.
- `zoneId`: required stable zone ID. Day 09 validates syntax; Day 12 cross-validates it against `BattleZones.json`.
- `roleId`: validated ambient Civilian role.
- `actorClass`: soft class path deriving from `AAuraCivilian`.
- `spawnVolumeIds`: one or more unique Civilian spawn-volume IDs.
- `initialCount` and `maximumCount`.
- `defaultWorkProfileId`: required ID from `CivilianWorkProfiles.json`.
- `memberOverrides`: optional array keyed by unique `slotIndex` within `0 .. maximumCount - 1`; each override may replace `workProfileId` and may supply one `merchantDefinitionId`. No row-level merchant default exists, so one shared population row cannot accidentally make every slot a merchant. Day 09 validates merchant-ID syntax only; Day 15 cross-validates non-empty per-slot values after merchant definitions exist.
- `spawnOnLoad`.
- `respawnPolicy` with `enabled`, `delaySeconds`, `corpseSeconds`, and allowed battle-phase names. Day 09 parses and validates this data but does not execute it until Day 13.

`CivilianWorkProfiles.json` is also versioned on Day 09. Its first fixture defines `Observer` with movement/threat/schedule values required by Day 10. Day 09 validates duplicate IDs, numeric ranges, and required fields; Day 10 consumes the profile behavior.

Validation reports all errors in one pass and rejects the complete table on authority if any row is unsafe. Validate:

- Supported schema versions and required root arrays.
- Duplicate population, work-profile, and spawn-volume IDs.
- Valid `mapId` and current-map filtering.
- Nonempty syntactically valid `zoneId`; unique/in-range member override slot indices; valid override work profiles; and syntactically valid optional per-member `merchantDefinitionId`.
- Existing validated role with ambient Civilian entity/control profile.
- Loadable actor class deriving from `AAuraCivilian`.
- `0 <= initialCount <= maximumCount` and a positive maximum.
- At least one referenced spawn volume registered for a row that spawns on load.
- Existing work-profile ID.
- Finite, nonnegative delays/radii and supported phase names.
- A role/class pair that cannot grant offensive abilities.

Unknown schema versions, partial rows, and invalid asset paths fail closed; they do not spawn a partially valid subset.

## Deterministic member identity

- Every population owns slots `0 .. maximumCount - 1`.
- Member ID is the canonical string `PopulationId:SlotIndex`; for example `MarketCivilians:3`.
- Each slot resolves its immutable work/merchant data from the row default plus that slot's optional `memberOverrides` entry. Another slot in the same row does not inherit its merchant ID.
- Initial spawn fills the lowest available slot indices.
- A Day 13 refill reuses the vacant slot and therefore the exact same member ID.
- Never derive member ID from actor name, pointer, spawn order across populations, or a random GUID.
- Registry uniqueness is keyed by the canonical member ID. A slot can have at most one live or pending actor.
- The deterministic scheme and schema version are included in the Day 13 debug snapshot and Day 18 persistence record.

## Exact load and spawn ordering

1. `AAuraGameModeBase::InitGame` first completes Day 05 RoleConfig publication, then on authority constructs `UAuraPopulationManager` with GameMode as its outer and calls `InitializeDefinitions` exactly once.
2. The manager loads and validates `CivilianWorkProfiles.json` and `PopulationSpawnTable.json` without spawning. Initialization failure keeps population readiness false before any Actor `BeginPlay`.
3. GameMode enters `StartPlay` with the manager already available, then calls `Super::StartPlay`; every `AAuraCivilianSpawnVolume::BeginPlay` registers its stable volume ID idempotently. No placed actor must win an unspecified BeginPlay race.
4. After `Super::StartPlay`, the manager performs idempotent world-discovery reconciliation for any placed volume that did not register, rejects duplicate IDs, and schedules one next-tick `FinalizeInitialPopulation`. Day 12 later inserts director/zone cross-validation before this finalization without changing ownership.
5. At finalization, verify definitions, required volumes, navigation data, and the manager generation before committing any actor.
6. For each current-map row, reserve a deterministic slot, ask referenced volumes for collision-free nav-projected candidates, and use `SpawnActorDeferred`.
7. Before `FinishSpawning`, set the authority-only requested role ID plus the complete `FAuraPopulationMemberState`. Day 08/06 then publish the distinct replicated `FAuraAppliedRoleState` during spawn initialization.
8. After `FinishSpawning`, verify Day 08 role/identity/ASC initialization and only then commit the actor to the registry.
9. On failure, release the slot and try another candidate up to a configured bounded attempt count. If the requested initial count cannot be reached, fail the Day 09 gate with population/slot/rejection diagnostics.
10. Definition initialization, volume registration/discovery, and finalization are generation-guarded so duplicate lifecycle/test calls cannot spawn a second population.

The RoleConfig editor reload sentinel does not reload or reconcile population data. A future population-reload tool must be a separate transactional feature.

## Implementation steps

1. Add the reflected population/work/slot structures and safe JSON loaders.
2. Add aggregated validation with row/field/error diagnostics.
3. Add the GameMode-owned `UAuraPopulationManager` and exact initialization sequence above.
4. Add the spawn volume with nav projection, collision overlap checks, bounded candidate attempts, and debug visualization.
5. Add the complete replicated read-only `FAuraPopulationMemberState` to `AAuraCivilian`.
6. Add one `MarketCivilians` fixture on the test map with three initial slots, five maximum slots, `Observer` default work profile, and no member merchant override. Day 15 adds a merchant override to one exact slot.
7. Add registry dumps showing generation, row, slot, member ID, actor, role, map, zone, work profile, and state.
8. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day9.PopulationSchemaValidation`
- `Aura.RoleBattle.Day9.WorkProfileValidation`
- `Aura.RoleBattle.Day9.DeterministicMemberIds`
- `Aura.RoleBattle.Day9.InvalidRoleClassRejected`
- `Aura.RoleBattle.Day9.DuplicateInitialization`
- `Aura.RoleBattle.Day9.SpawnFailureReleasesSlot`
- `Aura.RoleBattle.Day9.PreStartPlayManagerAvailability`
- `Aura.RoleBattle.Day9.MemberStateReplication`
- `Aura.RoleBattle.Day9.PerMemberOverrideIsolation`

The deterministic-ID test must create rows in different iteration orders and prove that every `PopulationId:SlotIndex` remains unchanged.

## Listen-server and dedicated-server smoke

`RunRoleBattleDay9NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. In each mode:

1. Load `RoleBattleCivilianTest` and wait for the one authoritative initialization generation.
2. Assert the configured initial count, unique canonical member IDs, valid role identities, and no overlapping/nav-invalid actors.
3. Compare the complete population/slot/member/work/zone/merchant member state plus the distinct applied-role state on two clients.
4. Connect a late client and verify it receives the same actors, all six member fields, and role state without triggering a spawn.
5. Invoke the initialization entry point again and prove actor count and registry generation do not change.
6. Restart the match and verify a fresh world creates exactly the configured initial count with the same deterministic IDs.
7. Confirm no death refill occurs on Day 09.

The runner enforces bounded startup, assertion, late-join, restart, and teardown timeouts; returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure; and stops only processes it created. It writes `Saved/Logs/Day09-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day09-{Listen|Dedicated}.json` with revision, commands, exit codes, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day9; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day09Automation.log'

& '.\RunRoleBattleDay9NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay9NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

One server-only `UAuraPopulationManager` exists before Actor BeginPlay and owns a fully validated/versioned population table, deterministic `PopulationId:SlotIndex` slots, idempotent volume registration/discovery, and initial Civilian spawning. Existing and late clients receive the complete six-field member state plus distinct role state, duplicate initialization cannot duplicate actors, and no temporary GameMode registry or population-reload sentinel behavior exists. All build, native, listen, and dedicated gates pass.
