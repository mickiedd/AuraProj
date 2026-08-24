# Role/Battle plan guardrails and runtime alignment

Date: 2026-08-24
Status: Complete; detailed Day 10-12 completion matrices remain explicitly open.

## Intent

Resolve the gaps found while reviewing the recent Role/Battle plan changes: implement the newly required fail-closed startup behavior, add its missing regression, add numeric coefficient coverage, make network baselines exercise meaningful runtime behavior, and ensure plan status does not overstate milestone completion.

## Changed behavior

- Added one GameMode-owned world-readiness state. Role, dispatcher, battle-zone director, population definitions, registered volumes/markers, and initial population must agree before the world becomes Ready.
- Cross-validates population map/zone references and current-map volume/marker registrations against `BattleZones.json`.
- Rolls back all newly spawned Civilians when initial population finalization cannot reach its declared count.
- Failed initial configuration publishes `Unhealthy`, rejects subsequent `PreLogin`, and never schedules the dedicated-server GSM ready notification.
- Resolves battle-zone map identity through the same configured map ID used by the population manager.
- Added numeric `1.0f` missing-table/missing-row regression coverage for ArmorPenetration, EffectiveArmor, and CriticalHitResistance coefficients.
- Added `Aura.RoleBattle.Day12.InitialFailurePublishesUnhealthy` and raised the Day 10-12 inventory to 32 native tests.
- Enriched each Day 10-12 network probe with real authority state: three running Civilian trees and markers, a real Civilian death transition with duplicate rejection and AI stop, and one-director/forged-context/phase-policy checks.
- Both clients must observe the director and all three population member states; reports now contain eight assertions per run.
- Updated the master plan, implementation index, Day 4/10/11/12 contracts, and issue dispositions. Baseline probes are explicitly separated from the broader completion matrices.

## Validation

- AuraEditor Win64 Development build passed after the final source changes.
- `Automation RunTests Aura.RoleBattle.Day` passed 109/109 with 0 failures; 32/32 were Days 10-12.
- `python Scripts/test_role_battle_days_10_12.py` passed with all 32 named tests.
- Day 10, Day 11, and Day 12 listen-server baselines passed with eight assertions each.
- Day 10, Day 11, and Day 12 dedicated-server baselines passed with eight assertions each.
- Each server reported coordinated `WorldReadiness=Ready`, three live population members, and its day-specific authority probe.
- `git diff --check` passed apart from normal CRLF normalization warnings.

## Remaining completion work

The plans intentionally keep the detailed threat/flee and navigation matrix, Player/Enemy reward and attributed-damage matrix, delayed-projectile boundary movement, allowed Civilian casualty, and complete damage-producer matrix open. Day 13 remains blocked until those acceptance matrices pass.

## Illustration

[Plan guardrails and coordinated startup flow](2026-08-24-role-battle-plan-guardrails.svg)
