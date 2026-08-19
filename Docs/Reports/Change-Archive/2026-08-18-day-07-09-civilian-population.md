# Day 7–9 role and Civilian population implementation

## Intent

Finish the role regression gate and establish the first authoritative Civilian vertical slice without adding Civilian AI, death refill, interaction, or economy behavior.

## Changed behavior

- Added schema-v1 `PopulationSpawnTable.json` and `CivilianWorkProfiles.json` fixtures.
- Added `AAuraCivilian` with one replicated ASC, one AttributeSet, authority-only role application, empty offensive loadout, protected default combat policy, and replicated population member state.
- Added `UAuraPopulationManager`, spawn-volume candidate providers, deterministic `PopulationId:SlotIndex` identities, deferred role/member assignment, validation, reservation rollback, and generation guards.
- Initialized the manager from GameMode `InitGame` before `StartPlay` and finalized after volume registration on the next tick.
- Added Day 7–9 native automation contracts, static checks, bounded runner scripts, and a StartupMap-based network fixture probe.
- Added the `AbilityDefinitions` UFS staging rule and a stable `RoleBattleCivilianTest` level ID mapped to the existing StartupMap fixture.

## Validation

- `AuraEditor Win64 Development` build passed.
- Day 7: 10 named native automation tests passed; focused Listen/Dedicated runners passed.
- Day 8: 6 named native automation tests passed; Listen and Dedicated Civilian replication smokes passed.
- Day 9: 9 named native automation tests passed; Listen and Dedicated three-member population smokes passed with deterministic member IDs.
- `python Scripts/test_role_battle_days_7_9.py` and `git diff --check` passed.
- The full BuildCookRun attempt was not counted as a pass: initial target inference failed, and the corrected client-target cook exceeded the bounded five-minute window while compiling engine modules. The packaged runner remains available for a completed archive path.

## Illustration

[Open the Day 7–9 implementation flow](2026-08-18-day-07-09-civilian-population.svg)
