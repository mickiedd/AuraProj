# Day 47A — Deterministic Offer Contract Harness

Status: Implemented as an inert validation milestone; the full Day 47 Run Augments plan remains Planned.
Depends on: Day 46 runtime role/status/supply consumers, Day 43 two-cell completion events, and Day 42 profile isolation.
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md), and [Day 47 plan](Day-47-Run-Augments.md).

## Milestone intent

Advance the next dependency boundary without pretending that the unimplemented Day 46 consumers exist. The test-only harness freezes Day 47's eight-entry offer catalog, deterministic seeded sampling, owner privacy, two generation-bound cell offers, replay, timeout, and reconnect semantics using the real Day 42/43 mission state reducer.

## Included

- `FAuraDay47ContractHarness` and `AuraGameplayDay47Tests` under the private test tree.
- The exact eight architecture IDs with explicit role eligibility and bounded parameter values, kept in test-only code until the Day 46 consumers are real.
- Three distinct offers sampled without replacement from the six-entry eligible pool for Aura or BungeeMan.
- Actual first and second Clear-cell completion results from `FAuraMissionRunState`; duplicate, stale-generation, and wrong-sequence boundary events cannot mint another offer.
- One stored choice per boundary, exact request replay, a 20-second visible deadline with first-listed auto-selection, and an owner-only value snapshot retained across the no-op reconnect seam.
- Offline checks in `Scripts/test_gameplay_day47_contracts.py` that keep the full Day 47 production files and runtime entry gate deferred.

## Explicitly deferred

This milestone does not create `GameplayAugmentDefinitions.json`, a production augment component, network/RPC offer delivery, UI choice cards, GAS effect handles, any of the eight live modifiers, `field_medic` medkit behavior, `steady_hands` Exposed behavior, reconnect persistence, or packaged-play evidence. Those require the full Day 46 role/status/supply implementation and its live input consumers first.

The owner snapshot is a deterministic test projection, not a replicated gameplay contract. The Day 46 atomic Exposed selection/consumption and run-supply ownership/reconnect contracts remain prerequisites for activating Day 47 effects.

## Validation gate

The Day 47A offline suite must pass with the Day 42–46 definition/contract suites. Native automation may validate the test-only harness when UE5.5 is available, but neither this harness nor its receipts satisfy the packaged gameplay, anchor survey, public-input, or independent-review gates. `DAY40_PACKAGED_EVIDENCE_MISSING` and `ANCHOR_SURVEY_UNVERIFIED` remain fail-closed.
