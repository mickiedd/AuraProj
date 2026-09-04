# Day 47A — Deterministic offer contract harness

Date: 2026-09-03. Scope: a dependency-safe, inert offer-boundary milestone for the gameplay-expansion Days 41–60 project. The full Day 47 Run Augments plan remains `Status: Planned`.

[Visual summary](2026-09-03-gameplay-expansion-day-47a-offer-contract.svg) · [Day 47A scope note](../../Plans/Gameplay-Expansion-Implementation/Day-47A-Offer-Contract-Harness.md) · [full Day 47 plan](../../Plans/Gameplay-Expansion-Implementation/Day-47-Run-Augments.md)

## Handoff and intent

The requested in-app ChatGPT handoff was attempted through the handoff skill. The local host was locked and automatic unlock failed, so no consultation result was available; the dependency audit continued locally. Day 47A was selected because the full Day 47 plan depends on Day 46 role, status, and run-supply consumers that are still planned. This slice freezes the offer boundary without inventing those consumers.

## Before / after

Before, Day 47's eight architecture IDs existed only in the plan and its runtime offer path depended on unimplemented Day 46 behavior. After, a private `FAuraDay47ContractHarness` provides a deterministic, test-only projection of that boundary:

- The exact eight IDs and bounded values are catalogued: four shared entries, two Aura entries, and two BungeeMan entries.
- Real `FAuraMissionRunState` Clear-cell completion results drive exactly two generation-bound offer boundaries. Duplicate keys, stale generations, wrong completion sequences, and excess boundaries are rejected.
- Each boundary samples three distinct entries without replacement from the six-entry role-eligible pool using a deterministic seed. An owner-only value snapshot carries the offer revision and a server-time deadline of 20 seconds.
- One choice is accepted per boundary. Exact request replay is idempotent, stale revisions and unlisted IDs are rejected, the first visible entry is selected at timeout, reconnect does not reroll, and a foreign owner receives no offer IDs.
- Offline contract checks and eight native automation tests make the boundary executable without creating production augment config, component, UI, RPC, GAS effect, medkit, Exposed, or packaged-runtime behavior.

## Validation

- `python3 Scripts/test_gameplay_day47_contracts.py`: 4/4 passed.
- `python3 Scripts/test_gameplay_day46_contracts.py`: 4/4 passed; Days 42–43 definitions: 4/4; Days 44–45 definitions: 11/11.
- Focused native automation `Aura.Gameplay.Day47A`: 8/8 passed, exit code 0, macOS queue-empty completion, and zero native log warnings/errors. Evidence: [`native-result.json`](../../../Saved/Reports/Day47A-Native-final.4yBZhr/native-result.json).
- `./BuildEditor.command`: UE5.5 UHT, compile, link, and macOS editor deployment passed. Targeted evidence-parser regressions: 4/4; native-warning policy: 11/11; Python syntax compilation passed.
- SVG/XML, archive-link, and focused diff checks passed. The default Git diff still requires the configured Git LFS filter, but `git-lfs` is not installed; the focused diff check disabled that missing filter explicitly.

## Gate classification

Day 47A is complete as a source-level validation milestone. The full Day 46 role/status/supply runtime, Day 47 production definitions and consumers, public input checkpoint, packaged gameplay evidence, anchor survey, and independent review remain deferred or fail-closed. `BLOCKED_RUNTIME_ENTRY`, `DAY40_PACKAGED_EVIDENCE_MISSING`, and `ANCHOR_SURVEY_UNVERIFIED` remain intentionally unchanged.
