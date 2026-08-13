# Role/Battle Day 03 — Combat Rules and Life State

Date: 2026-08-09  
Status: Implemented and verified.

## Plan intent

Provide one authoritative answer for combat targeting and damage permission, and add the minimum replicated life-state foundation needed to reject an actor as soon as death begins. Interaction remains a separate future contract.

## Finished work

- Added `AuraCombatRules` with structured relationship, permission, and stable rejection-reason results.
- Added trusted rule context and policy snapshot types. Production defaults deny PvP and Civilian damage unless a later authoritative resolver supplies policy.
- Added the replicated `UAuraCombatStateComponent` and the `Alive → Dying → Dead` state path plus replacement-pawn `Respawning → Alive` initialization.
- Made repeated entry into `Dying` idempotent and kept old dead pawns separate from replacement pawn initialization.
- Replaced the Day 02 relationship table with an `IsNotFriend` compatibility wrapper over the shared rule surface.
- Kept Enemy AI's existing blackboard contract while requiring identity plus `CanCombatTarget`.
- Added eight focused native tests and a listen/dedicated two-client network runner.

## Important guard

Clients cannot author life state or permissive policy. Missing/invalid policy is conservative, every non-`Alive` source or target is rejected, Civilian damage defaults to denied, and the only permissive pre-Day-12 policy factory is compiled for authority-side automation tests.

## Validation retained by the project

- Focused Day 03 native suite: `8/8` passed.
- Combined Day 01–03 Role/Battle suite at the recorded gate: `14/14` passed.
- Day 01 runtime regression and Day 02 network regression: passed.
- Day 03 Listen and Dedicated reports: state replication, client mutation denial, Civilian default denial, and Enemy targeting all passed.
- The 2026-08-13 full `Aura` rerun found `35` tests and exited `0`; all eight Day 03 tests passed again.

## Carryover

Day 12 remains the only production milestone allowed to resolve a permissive battle-zone policy. Day 11 later expands the minimal state machine into role-specific exactly-once death policies; Day 03 does not claim that later lifecycle work.

## Sources

- [Day 03 plan](../../Plans/Role-Battle-Implementation/Day-03-Combat-Rules.md)
- Retained reports: `Saved/Reports/Day03-Listen.json` and `Saved/Reports/Day03-Dedicated.json`
- [Mind map](2026-08-09-role-battle-day-03-combat-rules.svg)
