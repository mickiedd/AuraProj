# Role/Battle Day 04 — Authoritative Damage Boundary

Date: 2026-08-10  
Status: Implemented and verified.

## Plan intent

Route every production damage producer through the same authoritative combat-rule boundary, validate a second time during execution, and preserve attribution across direct, projectile, beam, radial, hitscan, melee, and periodic/debuff paths.

## Finished work

- Completed the checked-in producer inventory and classified native, AuraAbilityGraph, Blueprint/snapshot, and test-only paths.
- Extended damage parameters and Gameplay Effect context with ability tag, damage type, source role, controller/PlayerState, and future battle zone/event IDs.
- Routed native `CauseDamage`, projectiles, FireBall, and graph builders/actions through `ApplyDamageEffect`.
- Added a first authority/rule/state check before spec creation and a second check inside `ExecCalc_Damage` before `IncomingDamage` is emitted.
- Preserved accepted damage semantics: block, critical, resistance, knockback, radial falloff, debuff, and hit reaction.
- Made delayed/periodic damage retain original attribution and revalidate current life state and policy on each effective tick.
- Added ten Day 04 native tests and a listen/dedicated invalid-client-damage smoke runner.

## Important guard

A client, invalid ASC/avatar, friendly/protected target, or non-`Alive` source/target changes no authoritative Health. A graph node may prefilter for efficiency, but it cannot bypass the shared boundary; periodic damage also stops when the current server state or policy no longer permits it.

## Validation retained by the project

- Day 04 native automation: `10/10` passed.
- Producer inventory/table agreement and attribution serialization tests: passed.
- Prior-day regressions and AuraAbilityGraph smoke: passed at the recorded milestone.
- Listen and Dedicated damage reports both record the invalid client request was sent, received, rejected, and left server Health unchanged.
- The 2026-08-13 full `Aura` rerun found `35` tests and exited `0`; all ten Day 04 tests passed again.

## Carryover

Day 12 will populate trusted battle zone/event context. Later producer changes must update both the inventory and its compile-time test table. Rendered FireGun presentation still belongs to the Day 07 gate; Day 04 proves the server damage and attribution path.

## Sources

- [Day 04 plan](../../Plans/Role-Battle-Implementation/Day-04-Damage-Migration.md)
- [Damage producer inventory](../Role-Battle-Damage-Producer-Inventory.md)
- Retained reports: `Saved/Reports/Day04-Listen.json` and `Saved/Reports/Day04-Dedicated.json`
- [Mind map](2026-08-10-role-battle-day-04-damage-boundary.svg)
