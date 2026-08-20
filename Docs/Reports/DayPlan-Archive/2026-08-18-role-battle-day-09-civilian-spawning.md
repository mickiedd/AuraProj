# Role/Battle Day 09 - Deterministic Civilian Population Spawning

Date: 2026-08-18  
Status: Implemented and verified.

## Plan intent

Make one server-owned population manager load a validated versioned table, discover spawn volumes idempotently, and create stable Civilian members with deterministic identities and replicated state.

## Finished work

- Added versioned population and work-profile fixtures with aggregate validation that rejects an unsafe table rather than publishing partial rows.
- Added `UAuraPopulationManager` initialization from GameMode `InitGame`, deferred finalization after volume registration, generation guards, and reservation rollback.
- Assigned canonical `PopulationId:SlotIndex` member identities and kept population ownership separate from existing enemy respawn ownership.
- Added late-join replication and duplicate-finalization assertions for the complete member state without refill or respawn side effects.

## Important guard

Only the server loads, validates, reserves, and spawns population members. Volume registration and initialization are idempotent; failed reservations roll back; a generation mismatch cannot publish stale actors or duplicate slots.

## Validation retained by the project

- Day 09 native automation: `9/9` passed.
- Listen and Dedicated smokes passed with `3/3` canonical server slots, volume registration, duplicate-initialization stability, `3/3` client members, late-join replication, and no refill marker.
- The final Day 07–09 staged packaged topology and structural checks passed; `git diff --check` passed.

## Carryover

Day 10 adds server-authoritative Behavior Tree work, threat detection, flee behavior, and shelter selection. Death policies, battle zones, economy, and persistence remain later milestones.

## Sources

- [Day 09 plan](../../Plans/Role-Battle-Implementation/Day-09-Civilian-Spawning.md)
- [Day 07–09 implementation record](../Change-Archive/2026-08-18-day-07-09-civilian-population.md)
- [Day 07–09 test coverage record](../Change-Archive/2026-08-18-day-07-09-test-coverage-gate.md)
- [Listen validation report](../../../Saved/Reports/Day09-Listen.json)
- [Mind map](2026-08-18-role-battle-day-09-civilian-spawning.svg)
