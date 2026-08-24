# Role Battle Day 11 – Death lifecycle

Date: 2026-08-21
Status: Source/data implementation complete; runtime network gate remains explicit carryover.

## Finished flow

`UAuraCombatStateComponent` is the single Alive-to-Dying winner and creates a death sequence/event with source, policy, impulse, and battle attribution. `UAuraDeathPolicyDispatcher`, owned by GameMode, deduplicates the event and routes PlayerRespawn, EnemyLoot, and PopulationRespawn consequences. Enemy loot/XP/lifespan now sit behind the accepted policy event; Civilian death is recorded once and refill remains deferred to Day 13.

Replicated life state and death sequence remain durable client truth, while the existing multicast remains presentation-only.

## Validation

- AuraEditor build passed.
- Day 11 native namespace contains all 8 planned test names.
- Repeated-event source guards, policy routing, attribution, and non-Alive combat checks are covered by static/native contracts.
- Direct Unreal automation did not produce a log in this environment and is not claimed as passed.

## Mind map

[Day 11 flow](2026-08-21-role-battle-day-11-death-lifecycle.svg)
