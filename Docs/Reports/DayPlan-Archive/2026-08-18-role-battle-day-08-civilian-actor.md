# Role/Battle Day 08 - Replicated Civilian Actor

Date: 2026-08-18  
Status: Implemented and verified.

## Plan intent

Introduce the first Civilian actor shell with role-derived identity, replicated presentation and population state, initialized GAS health, and no offensive loadout.

## Finished work

- Added `AAuraCivilian` with one replicated ASC and AttributeSet, authority-only role application, positive vitals, and replicated population-member state.
- Reused the validated role/application path while keeping Civilian outside the Player/Enemy actor model and without adding a new character class.
- Made the default Civilian combat policy protected and its offensive grant set explicitly empty.
- Added server initialization and client/late-join presentation assertions to the Day 08 network probes.

## Important guard

Civilian identity and loadout are server-owned. A Civilian is not player-selectable, has no offensive ability or weapon inherited from a player, and damage remains denied by default until the later authoritative battle-zone policy exists.

## Validation retained by the project

- Day 08 native automation: `6/6` passed.
- Listen and Dedicated Civilian smokes passed with server identity/ASC initialization, empty offensive loadout, client presentation, replication, and no-crash assertions.
- The final Day 07–09 staged topology also passed packaged profile, mutation, and respawn checks.
- Static contracts, PowerShell syntax, Python checks, and `git diff --check` passed.

## Carryover

Day 09 owns authoritative population data loading, deterministic member IDs, volume registration, and initial spawning. Civilian AI, death refill, interaction, and economy remain later milestones.

## Sources

- [Day 08 plan](../../Plans/Role-Battle-Implementation/Day-08-Civilian-Actor.md)
- [Day 07–09 implementation record](../Change-Archive/2026-08-18-day-07-09-civilian-population.md)
- [Day 07–09 test coverage record](../Change-Archive/2026-08-18-day-07-09-test-coverage-gate.md)
- [Listen validation report](../../../Saved/Reports/Day08-Listen.json)
- [Mind map](2026-08-18-role-battle-day-08-civilian-actor.svg)
