# Day 25 — Authoritative Ammunition Model

Status: Planned  
Depends on: Day 22 baseline and Day 21 scope contract

## Goal

Turn BungeeMan's active `FireGun.xml` route into a complete minimum firearm loop with server-owned magazine and reserve ammunition.

## Work

- Extend the weapon/ability definition contract with magazine size, reserve capacity, shot consumption, reload duration/state, and explicit default values.
- Keep `FireGun.xml` as the sole active grant path and retain its existing damage/attribution boundary.
- Store authoritative ammo on the player-owned replicated state that survives pawn replacement according to the persistence contract.
- Add a narrow reload request through the owning player endpoint; validate ownership, role/loadout, state, timing, and capacity on the server.

## Detailed execution contract

### Files to inspect or modify

- **Active ability/config:** `Content/AbilityDefinitions/FireGun.xml`, `Content/Config/RoleConfig.json`, and the XML/config parser and validator.
- **Runtime authority:** `Source/Aura/Public/AbilitySystem/Abilities/AuraFireGun.h/.cpp`, the active AuraAbilityGraph execution path, `AuraPlayerController`, `AuraPlayerState`, and the pawn replacement/role grant ledger.
- **Persistence/replication:** `Source/Aura/Public/Game/AuraPlayerSaveGame.h`, `AuraPersistenceSubsystem.h`, `AuraPersistenceManifestSaveGame.h`, and their implementations.
- **Planned tests/output:** `Source/Aura/Private/Tests/AuraRoleBattleDay25Tests.cpp`, `RunPlayableCandidateDay25Ammo.ps1`, `day-25-ammo.json`, and `Docs/Reference/Playable-Candidate-Ammo-Contract.md`.

### Ammo state contract

Add one player-owned `FAuraFirearmState` (or the existing equivalent) with validated `MagazineCapacity`, `MagazineRounds`, `ReserveCapacity`, `ReserveRounds`, `ReloadDuration`, `bReloading`, `ReloadSerial`, and `AmmoRevision`. Capacity and duration come from the validated FireGun definition; rounds and reload state are mutable only on the authoritative server. Replicate the state owner-only and persist the rounds/reload completion policy fixed on Day 21. The active XML path remains the only grant path.

### Detailed steps

1. Trace `FireGun.xml` from RoleConfig through the active data ability and confirm the compatibility `UAuraFireGun` helper cannot grant a second spec.
2. Add schema fields and validation for positive capacities, `0 ≤ rounds ≤ capacity`, nonnegative duration, and one source of truth; reject malformed data before publication.
3. Create or attach the firearm state to the stable player-owned state, initialize it after role/loadout validation, and replicate it only to the owning connection.
4. Implement server-side shot consumption: validate owner, Alive state, BungeeMan loadout, target request, cooldown, and `MagazineRounds > 0`; decrement once and publish a new revision only after the shot is accepted.
5. Implement reload as an owned reliable request with a server timer/serial; reject duplicate, stale, dead, wrong-role, full-magazine, and insufficient-reserve requests with typed results.
6. Define cancel/interrupt behavior for death, pawn replacement, disconnect, and a new shot; no client timer may complete a reload.
7. Restore state when the same PlayerState/pawn is replaced and when Day 31 loads a profile; verify the reload serial cannot duplicate rounds.
8. Add owner-only WebUI state events and rejection reasons for Day 26 without allowing WebUI to write the state.
9. Run native, config, listen, dedicated, and packaged checks and publish the ammo contract before Day 26.

### Named automation and commands

- Native tests: `Aura.RoleBattle.Day25.AmmoSchema`, `InitialState`, `OneShotConsumesRound`, `EmptyMagazine`, `ReloadCompletes`, `ReloadRejectsInvalid`, `PawnReplacement`, `OwnerOnlyReplication`, and `ForgedClientValuesRejected`.
- Run the focused native suite, the XML/graph smoke, `RunPlayableCandidateDay25Ammo.ps1 -Mode Listen`, and `-Mode Dedicated`; both modes are mandatory.
- The runner writes per-process logs and JSON with before/after ammo, request/result IDs, role, topology, persistence namespace, and exit code. Any assertion, timeout, crash, or missing artifact returns nonzero.

## Deep-review closure

- **Owner surfaces:** the active `Content/AbilityDefinitions/FireGun.xml` and `Content/Config/RoleConfig.json` path, `Source/Aura/Public/AbilitySystem/Abilities/AuraFireGun.h` and its implementation boundary, the authoritative role/pawn state, and the player persistence schema. The compatibility `UAuraFireGun` helper is not a second grant path.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Ammo-Contract.md`, a server/client state trace, and `day-25-ammo.json` covering magazine, reserve, reload, pawn replacement, and owner-only replication.
- **Gate:** under two-client listen and dedicated runs, only the server changes ammo; a valid press consumes exactly one round, ammo never becomes negative, reload cannot overlap an accepted shot, and pawn replacement/reconnect restores the frozen player-owned state without exposing it to the other client.

## Validation and evidence

- Native tests cover initial state, one shot, empty magazine, reserve depletion, reload completion/cancel, respawn, replication, and forged client values.
- AbilityGraph/config tests prove the active XML and data path have one grant and one source of truth.
- Listen/dedicated probes show both players receive only their own ammo state.

## Completion gate

FireGun cannot fire indefinitely unless configured to do so; clients display a server-reconciled ammo state and cannot create ammo or bypass reload.

## Defer

Do not add durability, attachments, weapon modifiers, or a generalized inventory-of-weapons system.
