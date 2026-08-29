# Playable Candidate plan consistency fixes

Date: 2026-08-29

## Intent

Review and close actionable ambiguities across the complete Playable Candidate Days 21–40 plan set, then apply the related local Game Server Manager safety fix.

## Changed behavior and contracts

- Frozen reward, restock, reload, recovery, role-applicability, topology, and cadence rules are explicit and server-authoritative.
- Aura's active `FireBolt` path is explicitly not a firearm-ammo path; BungeeMan owns the FireGun ammo/reload checkpoints.
- Scope revision and source revision are separate; Day 38 retains a provisional `candidate-draft.json`, Day 39 owns the explicit soak operation, and Day 40 alone publishes immutable `candidate.json` through `-Finalize`.
- Candidate sign-off rejects filtered/dirty evidence and requires all four local lanes; external provider readiness remains a separate `BLOCKED`/`PASS` result.
- The Game Server Manager now binds to `127.0.0.1` by default in the Python CLI and Windows/macOS launchers; LAN/public binding requires an explicit override and the planned access controls.

## Validation

- All 20 Day 21–40 plan files were scanned; downstream unresolved decision tokens and artifact placeholder mismatches were absent.
- All 16 Python contract scripts passed.
- `Scripts/GameServerManager.py` compiled successfully.
- Launcher loopback-default assertions passed.
- `git diff --check` and SVG/XML validation passed.
- No Unreal build or packaged runtime matrix was rerun; those remain execution work defined by the reviewed plans and existing release evidence.

## Visual summary

[Open the visual summary](2026-08-29-playable-candidate-plan-fixes.svg)
