# Day 7–9 runtime hardening

## Intent

Close the issues found during the deep review of the Day 7/8/9 implementation, including normal civilian population setup, role identity safety, network topology coverage, packaged validation, and the FireBolt self-overlap regression.

## Changed behavior

- Normal population setup now creates a bounded runtime civilian fixture, anchors it to navigation, projects spawn candidates to navigation, and checks pawn occupancy without treating the ground as a blocking pawn.
- Civilian combat identity is role-owned and fail-closed. Population validation now rejects hostile or combat-capable civilian definitions and validates typed optional fields, ranges, classes, levels, and per-member overrides.
- Day 7 Listen/Dedicated runners now assert the real server role-grant audit. Day 8/9 runners exercise the runtime population and replication paths.
- Packaged mode now starts the staged Windows client and server instead of using editor automation, verifies cooked definitions, validates authority/presentation/mutation/respawn behavior, and tears down launcher descendants.
- FireBolt keeps the source-avatar/owner/instigator self-overlap guard so a client overlap cannot create impact behavior on the caster.

## Validation

- AuraEditor, Aura, and AuraServer Win64 Development builds passed.
- 25/25 named Day 7/8/9 native automation tests passed.
- Day 7 Listen and Dedicated, Day 8 Listen and Dedicated, and Day 9 Listen and Dedicated smokes passed.
- Final staged packaged client/server topology passed, including five definition manifests, authoritative profiles, two clients, two respawns per role, mutation rejection, crash checks, and full process-tree teardown.
- Python structural checks, PowerShell parser checks, FireBolt graph checks, and `git diff --check` passed.

## Illustration

[Runtime hardening flow](./2026-08-18-day-07-09-runtime-hardening.svg)
