# Day 46A — Deterministic contract harness

Date: 2026-09-03. Scope: the next inert gameplay-expansion milestone after the Days 42–45 foundation. The full Day 46 role-kit plan remains Planned; this job establishes its validation boundary without activating runtime role behavior.

[Visual summary](2026-09-03-gameplay-expansion-day-46a-contract-harness.svg) · [Day 46A scope note](../../Plans/Gameplay-Expansion-Implementation/Day-46A-Contract-Harness.md) · [full Day 46 plan](../../Plans/Gameplay-Expansion-Implementation/Day-46-Role-Kits-and-Combos.md)

## Intent

Use the existing mission, encounter, attack-timeline, lease, status, and interrupt contracts as one deterministic test stream. The handoff consultation selected this Day 46A slice before the larger Role Kits and Combos work because it closes the cross-contract validation gap while preserving the project’s fail-closed runtime gates.

## Changed behavior

- Added a private `FAuraDay46ContractHarness` that takes explicit `RunId`, epoch, encounter generation, identities, and server timestamps; it drives the real Days 42–45 APIs and emits a sorted, value-only snapshot.
- Added nine native automation cases covering deterministic replay, encounter-generation isolation, source-life isolation, driver ownership, heavy-lease lifecycle convergence, support-owner promotion, attack replacement, terminal cleanup, and a cross-contract stale-callback storm.
- Added an offline source/config suite that confirms the five-source registry remains closed, the Day 46A harness stays test-only, the full role-kit assets remain deferred, and `DAY40_PACKAGED_EVIDENCE_MISSING` / `ANCHOR_SURVEY_UNVERIFIED` remain fail-closed.
- Added a dated Day 46A scope note so the implemented validation milestone cannot be mistaken for completion of the full Day 46 Role Kits and Combos plan.
- Fixed the real UE5.5 UHT error on the existing private replicated attack-timeline state by declaring its intended Blueprint private access explicitly.
- Fixed the real cross-platform evidence-parser gap for UE5.5 macOS queue-empty completion markers and added a regression fixture for the `LogMac` exit form.
- Resaved the six legacy Cartoon City material packages and `Login.umap` after Unreal confirmed their empty engine-version metadata; the focused run now starts without those warnings.

No production gameplay component, definition registry entry, role loadout, input binding, GAS/damage path, movement path, supply schema, or packaged-runtime evidence was added.

## Validation

- `python3 Scripts/test_gameplay_day46_contracts.py`: 4/4 passed.
- `python3 Scripts/test_gameplay_day42_43_definitions.py`: 4/4 passed.
- `python3 Scripts/test_gameplay_day44_45_definitions.py`: 11/11 passed.
- Targeted evidence-parser regression tests, including Windows and macOS queue-empty completion: passed.
- `python3 -m py_compile` for all three gameplay definition suites: passed.
- `./BuildEditor.command`: UE5.5 UHT, compile, link, and macOS editor deployment passed after the private replicated-state metadata fix; existing deprecation warnings remain non-blocking.
- Focused UE automation (`Aura.Gameplay.Day46`): 9/9 passed, 0 failed, exit code 0; macOS queue-empty completion; parser evidence PASS with 0 log warnings/errors in `Saved/Reports/Day46A-Native-final.MoucIG`.
- SVG/XML, archive-link, and focused diff checks passed. The repository-default `git diff` still requires the configured Git LFS filter, but `git-lfs` is not installed; the focused check used filters explicitly disabled.

## Gate classification

The harness and offline contracts are complete at source level. The full Day 46 role-kit behavior, live Exposed semantics, run supplies, input/runtime activation, and packaged evidence remain deferred. Existing `DAY40_PACKAGED_EVIDENCE_MISSING` and `ANCHOR_SURVEY_UNVERIFIED` blockers are intentionally unchanged.
