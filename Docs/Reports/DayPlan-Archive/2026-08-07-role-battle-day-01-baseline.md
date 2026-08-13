# Role/Battle Day 01 — Baseline

Date: 2026-08-07  
Status: Conditional headless gate passed; rendered presentation remains assigned to Day 07.

## Plan intent

Establish a reproducible baseline before changing role, combat, or death behavior. The work had to prove the active Aura and BungeeMan configuration, bidirectional combat, respawn behavior, and the shape of the existing damage surface.

## Finished work

- Selected `/Game/Maps/StartupMap` as the reproducible headless fixture and verified the configured Login map separately.
- Exercised Aura's FireBolt, FireBlast, ArcaneShards, and Electrocute paths and resolved BungeeMan's mesh, animation, rifle, `Muzzle`, `FireGun.xml`, and `fireGunBullet` configuration.
- Fixed repeated additive attribute initialization on the persistent PlayerState-owned ASC. Initial attributes now apply once; replacement pawns refill current Health and Mana without increasing maxima.
- Assigned zero defaults to aggregate SetByCaller attribute magnitudes before selective values are set, removing unassigned-magnitude errors.
- Added `RunRoleBattleDay1Smoke.bat` to prove both live damage directions and two stable replacement-pawn respawns.
- Captured the distributed damage/relationship surface that became the starting point for the expanded Day 04 producer inventory.

## Important guard

The persistent ASC is initialized once per PlayerState. Pawn replacement rebinds the ASC and refills current vitals, but does not reapply additive defaults. This changes the observed respawn sequence from `100 → 200 → 300` maximum Health to stable `100 / 100` Health and `50 / 50` Mana after both respawns.

## Validation retained by the project

- AuraEditor Development build: exit `0`.
- Native `Aura` automation: exit `0`.
- AuraAbilityGraph smoke: `22 passed, 0 failed` at the recorded Day 01 gate.
- Day 01 runtime smoke: Player-to-Enemy `100 → 90`, Enemy-to-Player `100 → 90`, then two respawns with unchanged vitals.
- `StartupMap` gameplay fixture and Login map load/map-check were recorded in the dated baseline report.

## Carryover

Rendered mesh, animation, rifle/muzzle FX, montage timing, and projectile presentation remain a Day 07 evidence gate. The incomplete initial damage inventory was expanded and checked in at the start of Day 04; the map does not misrepresent the remaining rendered check as complete.

## Sources

- [Day 01 plan](../../Plans/Role-Battle-Implementation/Day-01-Baseline.md)
- [Day 01 baseline report](../Role-Battle-Baseline-2026-08-07.md)
- [Mind map](2026-08-07-role-battle-day-01-baseline.svg)
