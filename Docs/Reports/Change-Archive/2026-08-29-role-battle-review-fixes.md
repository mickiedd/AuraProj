# Role/Battle review fixes — 2026-08-29

## Intent

Apply the real, reproducible findings from `Docs/Tracking/Role-Battle-Review-Findings-2026-08-28.md` without changing gameplay behavior where the review did not establish a defect.

## Changed behavior

- `RunReviewSmokeSuite2.ps1` now keeps runner results in a script-scoped typed collection, exports the same records it summarizes, and has a focused two-runner contract mode.
- `RunSmokeTest.bat` delegates to `RunSmokeTestSupervisor.ps1`, which uses a unique log path, a bounded 180-second process wait, Unreal's explicit exit-status API, and strict result-log checks.
- Day 7 listen/dedicated wrappers run Day 7 and Day 6 in isolated child PowerShell processes, preserve both statuses, and write composition reports instead of mislabeling prerequisite failures.
- The packaged topology gate discovers every XML under `Content/AbilityDefinitions`, and role-battle smoke runners use per-invocation world persistence IDs where fixed IDs could contaminate later evidence.
- Day 7's documented commands now provide explicit `-Mode Listen` and `-Mode Dedicated` values.

The collision-safe respawn fallback and the test-only ProducerInventory allowlist were already present in the current checkout; they were verified and left unchanged.

## Validation

- `build_test.bat` completed an AuraEditor Win64 Development build successfully.
- `RunSmokeTest.bat` completed with `26 passed, 0 failed` and wrapper exit code `0`; the previous fixed-log lock case now fails safely rather than consuming stale evidence.
- The review-suite contract retained two CSV rows and returned exit code `1` when one fixture runner failed.
- PowerShell parsing passed for all modified scripts; the focused review-fix contracts and Day 7–20 Python contracts passed; `git diff --check` passed.
- Full Day 3–17 multiplayer and packaged archive runs were not rerun in this pass because they require long-lived external Unreal process/package evidence.

Illustration: [role-battle-review-fixes.svg](./2026-08-29-role-battle-review-fixes.svg)
