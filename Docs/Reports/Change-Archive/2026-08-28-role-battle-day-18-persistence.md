# Role/Battle Day 18 - Player and World Persistence

Date: 2026-08-28

## Intent

Persist each authenticated profile independently from one authoritative world snapshot. Restore profiles before pawn role/loadout initialization, restore population and per-member merchant stock exactly once, and recover from torn or corrupt newest manifests without partially applying state.

## Changed behavior

- `UAuraPlayerProfileIdentity` canonicalizes provider plus unique ID, hashes save slot names, rejects missing or mismatched providers, and permits `OnlineSubsystemNull` identities only inside the explicit non-shipping Day 18 fixture probe.
- Versioned player, world, and alternating A/B manifest records now carry checksums, generations, validated IDs, deterministic ordering, wallet/inventory state, population snapshots, and merchant stock. Invalid newest records fail closed or fall back to the prior verified generation.
- Profile preparation and application happen before pawn role/loadout grants. Reconnects receive a new Day 17 session nonce; old nonces are rejected. Successful purchases checkpoint the committed player and world state together.
- Population snapshots reconcile known members exactly once, omit removed members with warnings, and default newly configured members. Merchant stock is restored before interaction becomes active.
- The legacy migration test now deserializes the checked-in binary `GVAS` version-zero fixture and preserves BungeeMan role, progression, and FireGun ability provenance without duplicating inventory.

## Validation

- `build_test.bat`: AuraEditor Win64 Development passed after fail-closed profile application, manifest checksum, fixture deserialization, and merchant probe hardening.
- All repository Python contracts: 13/13 passed.
- Focused native automation: 15/15 Day 18 persistence tests passed.
- Full `Aura` native automation: 198/198 tests passed, 0 failed.
- `RunRoleBattleDay18PersistenceSmoke.ps1 -Mode Listen`: passed two-profile purchase/checkpoint, reconnect wallet isolation, stale nonce rejection, duplicate identity rejection, world isolation, provider mismatch rejection, population/merchant restore, crash scan, and bounded teardown.
- `RunRoleBattleDay18PersistenceSmoke.ps1 -Mode Dedicated`: passed the same assertions.
- `git diff --check`: passed; only existing line-ending warnings were reported.

The live smoke uses the existing configured `StartupMap.umap` and explicit non-shipping fixture identities. Production persistence still requires the configured authenticated Online Subsystem provider; the fixture path cannot satisfy a Shipping login. Runtime logs and JSON reports are retained under `Saved/Logs` and `Saved/Reports`.

## Illustration

[Role/Battle Day 18 persistence and recovery flow](2026-08-28-role-battle-day-18-persistence.svg)

## Evidence

- Focused log: `Saved/Logs/Day18Automation-Focused-Fixture.log`
- Full-suite log: `Saved/Logs/AuraAutomation-Full-Day18-Final.log`
- Listen report: `Saved/Reports/Day18-Listen.json`
- Dedicated report: `Saved/Reports/Day18-Dedicated.json`
- Checked-in legacy fixture: `Source/Aura/Private/Tests/Fixtures/RoleBattleLegacyV0.sav`

## Completion gate

Day 18 persistence is implemented and verified end to end: validated per-player records are isolated from the world snapshot, load ordering prevents duplicate grants, committed commerce checkpoints restore exactly, population and merchant state reconcile once, and manifest recovery preserves the last valid generation.
