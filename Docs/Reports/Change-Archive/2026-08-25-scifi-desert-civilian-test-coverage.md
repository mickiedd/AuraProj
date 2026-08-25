# Scifi Desert civilian test coverage

## Intent

Add a durable regression test case for the Scifi Desert AI-Civilian level change and make the existing automation runner reliable on localized Unreal output.

## Changed test coverage

- Added `Scripts/test_scifi_desert_civilian_population.py`.
- The new test checks the saved level asset, exact `DesertVillageCivilians` data, all 32 ordered village volume IDs, central-city exclusion/topology script guards, native volume-bound synchronization, and optional commandlet/runtime evidence.
- Runtime evidence checks 32 volume registrations, 48 markers, 32 spawns, 32 Behavior Tree starts, successful population finalization, destination activity, and zero candidate/BT failures.
- Updated `RunRoleBattleDays789Automation.ps1` to accept both English and localized Unreal automation result tokens while retaining the exact expected test count.

## Validation

- New Scifi Desert regression test: PASS with topology and runtime logs.
- Civilian AI, Day 7-9, Day 10-12, FireBolt, Game Server Manager, editor routing, Python compilation, and diff checks: PASS.
- Day 7, 8, and 9 Listen automation: PASS.
- Day 10, 11, and 12 Listen network smokes: PASS.
- Packaged network checksum validation remains environment-blocked because the installed Unreal distribution rejects `AuraServer` targets; the strict test was not weakened.

Illustration: [Scifi Desert civilian test coverage](2026-08-25-scifi-desert-civilian-test-coverage.svg)
