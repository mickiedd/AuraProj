# Gameplay Expansion Day 49 — escort contract foundation

Date: 2026-09-04. Scope: the next dependency-safe source slice in the Gameplay Expansion Days 41–60 plan. This record covers the inert Day 49 civilian-rescue contract; it does not claim the full playable Day 49 runtime gate.

[Visual summary](2026-09-04-gameplay-expansion-day49-escort-contract.svg) · [Day 49 plan](../../Plans/Gameplay-Expansion-Implementation/Day-49-Civilian-Rescue-and-Space.md) · [native report](../../../Saved/Reports/Day42-49-Native-final/index.json)

## Handoff and intent

The requested in-app ChatGPT handoff was attempted through the handoff skill. The only exposed ChatGPT app was the protected Codex bundle (`com.openai.codex`); both app-name and explicit `/Applications/ChatGPT.app` access were denied or ambiguous under the host safety boundary. No consultant response was available, so the milestone continued locally with fail-closed scope and validation.

The intent was to advance the next milestone without introducing production escort authority ahead of the existing world-anchor, packaged-evidence, and runtime-entry gates.

## Changed behavior

- Added the inert authority-only `FAuraEscortReservationLedger` contract. It atomically leases exactly two live designated civilians to one RunId/Epoch and destination lease, rejects competing owners and stale life generations, and restores work on release.
- Added `FAuraEscortRunState` for preparation, exactly-once arrival, civilian-loss failure, terminal cleanup, and scoped mission-enemy damage policy. Player damage and unrelated/non-mission enemy targeting fail closed.
- Added the requested-movement stall reducer: actual route progress resets the clock; blocked requested movement requests repair at 5 seconds and 7 seconds and technically aborts by 9 seconds; out-of-range and intentional shelter waits pause only stall accumulation while the run deadline continues.
- Added `GameplayEscortDefinitions.json` with two fail-closed arrangement fixtures, two designated member IDs, the 800-unit follow range, and the 5/7/9-second stall policy.
- Added seven native contract cases covering ownership, damage scope, arrival idempotence, bounded stalls, intentional waits, release/work restoration, and two-role layout reachability.
- Kept the contract test-only: no production objective component, PopulationManager wiring, AI, HUD/RPC, live route survey, or packaged gameplay evidence was added.

## Validation

- `./BuildEditor.command`: UE 5.5 Mac Development UHT, compile, link, and deployment passed.
- Native `Aura.Gameplay`: 49/49 passed, 0 failed, 0 not-run, including the seven Day 49 cases. Evidence: [`index.json`](../../../Saved/Reports/Day42-49-Native-final/index.json).
- `python3 Scripts/test_gameplay_day49_contracts.py`: 3/3 passed.
- Dependency offline suites passed: Day 42–43 4/4, Day 44–45 11/11, Day 46 4/4, Day 47A 4/4, Day 48 3/3 (26/26 total).
- The separate legacy native-warning harness was attempted but is blocked by its strict host-path reparse check on `/var`; this does not change the Day 49 report. The Unreal log also retains pre-existing Git-LFS placeholder-asset warnings; those payloads were not rewritten.

## Remaining gates

The full Day 49 completion gate remains open: production objective/AI/HUD/RPC wiring, verified route and anchor survey, packaged gameplay evidence, and three-template play review are still required. `CONTRACT_ONLY`, `UNWIRED`, `DAY40_PACKAGED_EVIDENCE_MISSING`, and `ANCHOR_SURVEY_UNVERIFIED` remain intentionally fail-closed.

The required illustration is [archived here](2026-09-04-gameplay-expansion-day49-escort-contract.svg).
