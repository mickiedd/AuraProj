# Role/Battle Days 10-12 implementation

Date: 2026-08-21
Status: Source/data implementation complete; direct Unreal automation remains an explicit environment follow-up.

## Intent

Advance the Role/Battle slice through the next three plans: server-only Civilian activity/threat behavior, exactly-once death policy dispatch, and authoritative battle phase/zone resolution.

## Changed behavior

- Day 10 adds an immutable work-profile registry, authority-registered activity markers with deterministic reservations, native Civilian BT services/tasks/decorator, inverse `CanDamage(Candidate, Civilian)` threat sensing, shelter/flee/work/observe selection, and fixture markers including an unreachable shelter.
- Day 11 adds fatal/death attribution, replicated death sequence, an exactly-once GameMode-owned dispatcher, player/enemy/civilian policy routing, enemy loot/XP behind the policy hook, and deferred Civilian refill ownership for Day 13.
- Day 12 adds validated `BattleZones.json`, a replicated always-relevant `AAuraBattleDirector`, safe-zone/priority/lexical resolution, Peace/Alert/Conflict/Cleanup event lifecycle, and final-boundary overwrite/stamping of battle context.

## Validation

- `build_test.bat` — AuraEditor Win64 Development exit code 0.
- `python Scripts/test_civilian_ai.py` — pass.
- `python Scripts/test_role_battle_days_7_9.py` — pass.
- `python Scripts/test_role_battle_days_10_12.py` — pass, 31 named native tests present.
- PowerShell parser checks for all three network runners — pass.
- `git diff --check` — pass with normal line-ending warnings.
- Direct `UnrealEditor-Cmd.exe` Day 10-12 automation invocation returned exit code 1 before writing a log; no runtime/network result is claimed from that attempt.

## Carryover

The serialized legacy `BTS_FindNearestPlayer.uasset` and its compatibility C++ class remain until an Unreal Asset Registry/editor resave can safely migrate both enemy behavior-tree binaries to `BTS_FindNearestHostile`. The new native service and refreshed JSON snapshots are present and no new Civilian path uses the legacy service.

## Illustration

[Before/after implementation flow](2026-08-21-days-10-12-implementation.svg)
