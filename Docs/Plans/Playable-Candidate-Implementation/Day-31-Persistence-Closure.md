# Day 31 — Persistence Contract Closure

Status: Planned  
Depends on: Day 28 recovery, Day 29 economy loop, Day 30 restock policy

## Goal

Define exactly what survives session restart, server restart, map reload, pawn replacement, and reconnect, and make invalid data fail safely.

## Work

- Extend the versioned player/world schema for the already-frozen state: role/loadout, magazine/reserve ammo, tutorial progress, reward results, merchant stock/time, and player recovery state. No new persistence decision is permitted here.
- Specify migration, missing/corrupt data, partial-write, stale-manifest, and rollback behavior.
- Keep per-player identity separate from the authoritative world snapshot and preserve stable profile/world/member IDs.
- Update the persistence reference and candidate manifest with field ownership and lifetime.

## Detailed execution contract

### Files to inspect or modify

- **Persistence:** `Source/Aura/Public/Game/AuraPersistenceSubsystem.h`, `AuraPlayerSaveGame.h`, `AuraWorldSaveGame.h`, `AuraPersistenceManifestSaveGame.h`, profile identity types, and matching implementations.
- **Player state:** `AuraPlayerState`, role/grant ledger, wallet/inventory, firearm state, tutorial state, and recovery state.
- **World state:** `AuraPopulationManager`, `AuraMerchantComponent`, battle director/zone state, `WorldPersistenceId`, and merchant restock fields.
- **New planned output:** `Docs/Reference/Playable-Candidate-Persistence-Contract.md`, `Source/Aura/Private/Tests/AuraRoleBattleDay31Tests.cpp`, `RunPlayableCandidateDay31Persistence.ps1`, and `day-31-persistence.json`.

### Ownership and lifetime table

Player save owns stable validated profile identity, role/loadout, wallet, inventory, magazine/reserve ammo, tutorial completion, and player recovery state. World save owns `WorldPersistenceId`, generation, population/member lifecycle, battle phase/zone state, merchant stock, `LastRestockAtUtc`, and stock revision. Runtime pawn/UI/delegate/focus state is never serialized as authority state. A graceful checkpoint commits a complete snapshot; a forced kill restores the last valid manifest generation; uncommitted actions are rolled back.

### Detailed steps

1. Compare the Day 18 versioned player/world schemas and add only the frozen Day 25/29/30 fields with explicit schema versions.
2. Define field owner, lifetime, default, migration rule, privacy, and commit trigger for every added field; reject a field with no owner.
3. Preserve load order: provider/profile validation, role/config validation, world definitions/registry, world snapshot candidate, population/merchant reconciliation, PlayerState load, pawn/ASC grant.
4. Ensure reward, purchase, ammo, reload, recovery, and restock snapshots capture either pre-transaction or fully committed state, never intermediate mutation.
5. Implement legacy/version-zero, missing, corrupt, unknown-future, torn-manifest, failed-write, and stale-world cases with last-known-good fallback.
6. Test graceful restart, forced kill, map reload, pawn replacement, reconnect, late join, two-player isolation, world ID isolation, and merchant/population reconciliation.
7. Prove no pawn possession or client reconnect can trigger a second world restore, grant, starting balance, restock, or migration.
8. Publish schema/version/migration evidence consumed by Day 32 and Day 34.

### Named automation and commands

- Native tests: `FieldOwnership`, `AmmoRoundTrip`, `TutorialRoundTrip`, `RecoveryRoundTrip`, `LegacyV0Migration`, `FutureVersionRejected`, `CorruptManifestFallback`, `TornCheckpoint`, `ForcedKillRollback`, `PlayerIsolation`, `WorldLoadsOnce`, `WorldIdIsolation`, and `CommittedTransactionOnly`.
- Run `RunPlayableCandidateDay31Persistence.ps1 -Mode Listen` and `-Mode Dedicated` with two distinct profiles and isolated save roots.
- Any partial authoritative state, cross-player/world leak, duplicate restore, silent migration, or missing result file blocks Day 32.

## Validation and evidence

- Save → graceful shutdown → restore; forced-kill recovery; map reload; legacy version-zero migration; corrupt/missing/unknown-version fixtures.
- Verify two-player isolation, stable merchant/member identity, population reconciliation, and no partial UI/save event.
- Retain before/after values and machine-readable reports.

## Deep-review closure

- **Owner surfaces:** `Source/Aura/Public/Game/AuraPersistenceSubsystem.h`, `AuraPersistenceManifestSaveGame.h`, their implementations, economy components, role/grant ledger, and the Day 25/29/30 state contracts.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Persistence-Contract.md` with a player/world ownership-and-lifetime table, schema version/migration record, and `day-31-persistence.json` for graceful, forced-kill, migration, and corruption cases.
- **Gate:** role/loadout, wallet, inventory, ammo, tutorial completion, recovery state, merchant stock, and restock timestamp have one documented owner and lifetime; graceful restart restores all committed values; forced kill restores the last valid manifest generation; corrupt/unknown data fails closed without partial authoritative or UI state.

## Completion gate

Every candidate value has a documented owner and lifetime, migration is tested, and bad data cannot silently become partial authoritative state.

## Defer

Do not implement broad offline progression beyond the restock rule explicitly chosen on Day 30.
