# AuraProj Git daily mind-map index

This report is a manually written explanation of the `main` branch history for a newcomer. It groups commits by calendar date, compares each active day with the previous active-day checkpoint, and turns the diff into a nested mind map rather than repeating raw patch output.

## Scope and reading rules

- Branch reviewed: `main`, current tip `1c32b8e` (`Archive Git LFS snapshot size fix`).
- History reviewed: 172 reachable commits across 71 calendar days, from 2026-04-17 through 2026-08-11.
- Comparison unit: the final commit/tree for one active date versus the final commit/tree for the previous active date. Dates with no commits are not listed.
- Interpretation: generated assets, Blueprint snapshots, cache cleanup, runtime C++, editor code, configuration, tests, and documentation are grouped by the job they serve.
- Git topology: `2026-08-11` contains a real merge. Its asset/LFS side is `22573dc`; its incoming runtime-code side is `fb10b85`; `5fe427a` combines them. Same-subject commits visible only through `--all` and not reachable from `main` are not counted as extra jobs.
- Method: the maps and prose were authored directly after inspecting Git history and diffs. No script was used to generate this report.

## Project evolution at a glance

- Mind map: AuraProj over the reviewed period
  - Foundation
    - Unreal module/targets, editor module, initial Blueprints and GAS content
  - Playable session
    - Login map, player identity, dedicated-server startup, loading/travel route
  - Gameplay basics
    - movement, crouch, sprint, monsters, respawn, portals, camera, projectile replication
  - BehaviorU and vehicles
    - behavior editor/runtime, enemy agents, broom mount/follow/flight, autonomous AuraCar
  - Data-driven migration
    - JSON role/projectile/pickup/effect metadata
    - AuraAbilityGraph XML definitions and native action nodes
    - smoke tests, snapshots, migration audits
  - Role/Battle hardening
    - combat identity, combat rules/state, damage-producer inventory, safe damage boundary and attribution
  - Repository health
    - developer/cache cleanup, documentation organization, Git LFS for oversized runtime assets

## Daily maps

- [April 2026](Git-Daily-Mind-Maps-2026-04.md) — project foundation, login/bootstrap, identity, respawn, ability-input diagnostics.
- [May 2026](Git-Daily-Mind-Maps-2026-05.md) — multiplayer authority, snapshots, dedicated-server travel, camera, broom vehicle, mount animation.
- [June 2026](Git-Daily-Mind-Maps-2026-06.md) — building placement, BehaviorU foundation, network-loss detection, broom AI/component architecture.
- [July 2026](Git-Daily-Mind-Maps-2026-07.md) — autonomous car, showcase-level tools, headless automation, role config, AuraAbilityGraph migration.
- [August 2026](Git-Daily-Mind-Maps-2026-08.md) — data-driven pickups/projectiles/enemies, modular Electrocute, Role/Battle combat rules and damage hardening, LFS merge.
- [Full calendar coverage](Git-Daily-Calendar-Coverage-2026-04-17-to-2026-08-11.md) — explicitly accounts for the 46 dates with no `main` commits and therefore no jobs/diffs.
- [Last-seven-days supplement](Git-Daily-Changes-2026-08-06-to-2026-08-12.md) - manually expands 2026-08-06 through 2026-08-12, including the quiet 2026-08-12 row.
- [July 11-August 5 daily SVG supplement](Git-Daily-Changes-2026-07-11-to-2026-08-05.md) - adds one standalone illustration for every date, including eight quiet dates.

## How a newcomer should follow a feature

- Login/travel: start with April 26, then May 10–13, then May 25–26; the main owners are `LoginPlayerController`, `ULoginMenuWidget`, `GameServerClient`, `ServerTravelComponent`, `AuraGameInstance`, and `LevelJumpPortal`.
- Broom: start with May 18, then June 1–2 for mount state, June 21–28 for BehaviorU follow/flight, and July 17 for camera/lifetime hardening.
- Ability migration: start with July 26–31, then August 1–5; definitions live in XML/JSON, execution lives in AuraAbilityGraph/GAS/native nodes, and smoke/tests prove loading/execution.
- Role/Battle: start with August 7 baseline, August 8 identity, August 9 rules/state, and August 10 damage boundary. August 11 adds safe avatar/beam follow-up while also repairing storage for large runtime assets.
