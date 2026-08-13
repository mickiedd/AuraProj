# Role/Battle Day 02 — Combat Identity

Date: 2026-08-08  
Status: Implemented and verified.

## Plan intent

Replace raw Player/Enemy actor-tag decisions with an explicit replicated identity owned by each combat avatar. Transient projectiles and effect actors continue to use the source avatar's identity rather than inventing identities of their own.

## Finished work

- Added `FAuraCombatIdentity`: faction, control type, combat profile, death policy, and four explicit capability flags.
- Added one replicated `UAuraCombatIdentityComponent` to `AAuraCharacterBase`, with authority-only initialization, generic actor lookup, `OnRep_Identity`, validation, and rate-limited missing-identity diagnostics.
- Registered native Player, Enemy, Civilian, control, profile, and death-policy Gameplay Tags.
- Configured Player and Enemy defaults while retaining `Combat.Unassigned` as an explicit transitional profile.
- Reworked `IsNotFriend` into the bounded Day 02 identity truth table and changed enemy Player acquisition to require a valid, targetable `Faction.Player` identity.
- Added three focused native tests and a hidden-process replication smoke runner.

## Important guard

Only authority can set identity. There is no client setter or identity RPC, invalid or missing identity fails closed, and clients receive the complete four-tag/four-flag value through replication. No new Player/Enemy actor-tag combat checks were introduced.

## Validation retained by the project

- AuraEditor build: exit `0`.
- Focused Day 02 automation: `3/3` passed.
- Full native Aura regression at the recorded gate: `17/17` passed.
- Day 01 damage/respawn runtime regression: passed.
- Server/client replication smoke: passed; the client received matching Player and Enemy identities and Enemy AI retained its target.
- AuraAbilityGraph smoke: `22/22` passed.
- The stricter listen/dedicated two-client runner artifacts retained on 2026-08-13 report `Passed: true` for both modes.

## Carryover

`Combat.Unassigned` is valid only for this transitional milestone; Days 05–06 must publish and apply `Combat.Magic` or `Combat.Gun`. Pickup eligibility remains outside combat identity and is deferred to the Day 20 compatibility cleanup.

## Sources

- [Day 02 plan](../../Plans/Role-Battle-Implementation/Day-02-Combat-Identity.md)
- [Day 02 implementation report](../Role-Battle-Day-02-Combat-Identity-2026-08-08.md)
- [Mind map](2026-08-08-role-battle-day-02-combat-identity.svg)
