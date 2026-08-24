# Role/Battle pending-change review fixes

Date: 2026-08-24
Status: Complete

## Intent

Review the pending Role/Battle Days 10-12 implementation as an integrated runtime change, repair the discovered correctness gaps, and replace unexercised claims with build, native automation, and live network evidence.

## Changed behavior

- Migrated both serialized Enemy Behavior Trees and their Blueprint service from `FindNearestPlayer` to `BTS_FindNearestHostile`; refreshed snapshots now embed the actual saved packages.
- Added `MapId` to battle zones and made zone resolution map-specific. Configured permissions are authoritative only during `Conflict`; Peace, Alert, Cleanup, unknown maps, and locations outside a configured zone fail closed.
- Routed Enemy and Civilian target queries through the authority-owned battle director policy snapshot.
- Checked shelter path reachability before reserving it, allowing unreachable shelters to fall through to a projected flee destination.
- Corrected the runtime Civilian Behavior Tree's single decorator index from `1` to zero-based index `0`, eliminating the array-bounds crash found by the first network run.
- Replaced pointer-hash death deduplication with `FObjectKey` plus the highest accepted sequence per victim.
- Made the Day 10-12 smoke flags execute authoritative server probes and require replicated director observation from two clients.
- Kept the Day 10-12 native test source out of the production-source inventory gate and expanded behavior-focused coverage for phase, map isolation, policy, path fallback, and deduplication.

## Validation

- `AuraEditor Win64 Development` build passed.
- `python Scripts/test_role_battle_days_10_12.py` passed with all 31 named Day 10-12 tests present.
- `Automation RunTests Aura.RoleBattle.Day` completed 107 tests successfully with 0 failures; 31 were Days 10-12.
- Day 10, Day 11, and Day 12 listen-server smoke runs passed, each verifying its authority probe and replicated battle director on two clients.
- The initial Day 10 live run reproduced the decorator array-bounds crash; the corrected index was rebuilt and the same run then passed.
- Snapshot/package byte comparisons passed for both Enemy Behavior Trees and `BTS_FindNearestHostile`.
- `git diff --check` reported no whitespace errors (only existing CRLF normalization warnings).

## Illustration

[Before/after review fixes](2026-08-24-role-battle-review-fixes.svg)
