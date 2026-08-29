# Day 39 — Bounded Soak and Multiplayer Candidate Matrix

Status: Planned  
Depends on: Day 38 pipeline

## Goal

Prove within a bounded candidate run that repeated play does not accumulate process, persistence, replication, or UI state defects.

## Work

- Run repeated battle → reward → purchase → death → respawn → disconnect → reconnect cycles against packaged listen and dedicated builds.
- Measure server/client frame time, memory growth, population/merchant counts, replay-cache size, process cleanup, and artifact growth.
- Reuse per-run persistence namespaces and check for cross-run contamination.
- Define thresholds before the run and report warm-up, duration, failures, peak values, and unexplained warnings.

## Detailed execution contract

### Files to inspect or modify

- **Runner:** `RunPlayableCandidate.ps1` Candidate stage, packaged listen/dedicated launchers, and the existing bounded process supervisor.
- **State/metrics:** persistence namespace generator, server/client diagnostic writers, population/merchant counters, replay cache metrics, and process/artifact cleanup checks.
- **New output:** `Source/Aura/Private/Tests/AuraRoleBattleDay39Tests.cpp`, `RunPlayableCandidateDay39Soak.ps1` or an equivalent pipeline stage, `day-39-soak.json`, lane logs, and performance captures.

### Bounded soak contract

The mandatory run has four lanes: Aura/listen, BungeeMan/listen, Aura/dedicated, and BungeeMan/dedicated. Each lane completes 10 full cycles—battle, reward, purchase, death, recovery, disconnect, reconnect, save, and restart where applicable—with a 45-minute lane cap. An individual stage may not exceed 120 seconds and owned-process cleanup may not exceed 30 seconds. An overnight run is optional evidence, not a hidden requirement.

### Detailed steps

1. Freeze thresholds and warm-up policy in the candidate manifest before launching; create a fresh persistence namespace and artifact root for every lane/cycle.
2. Launch the packaged lane through Day 38, wait for readiness, and record build/package/revision/world/session identifiers.
3. Execute the complete cycle with two clients, including late join, simultaneous purchase/death, forced shutdown, restart recovery, and HUD replay at declared checkpoints.
4. Measure server/client frame time, memory, population/merchant counts, replay-cache size, process count, port state, and artifact growth after each cycle.
5. Assert no cross-run persistence, data loss, duplicate grant/stock, stale panel, population drift, replay leak, or orphan process.
6. At lane teardown, verify all owned children exit, ports release, reports close, and the next lane receives a new namespace and log set.
7. Compare warm-up versus final metrics; classify warnings as blocking or informational using the predeclared thresholds.
8. Publish machine-readable lane/cycle results and the aggregate only after every mandatory lane completes.

### Named automation and gate

- Mandatory thresholds: 10 complete cycles per lane within 45 minutes; no stage hang beyond 120 seconds; no owned process after 30 seconds; zero cross-run contamination, data loss, duplication, privacy leak, or stale UI; post-warm-up memory increase must remain ≤15%.
- Any missing cycle, unexplained monotonic artifact growth, blocking warning, or incomplete teardown returns nonzero and blocks Day 40.

## Validation and evidence

- Use two clients where available and include late join, simultaneous purchase/death, forced shutdown, and restart recovery.
- Check for hangs, orphaned children, stale WebUI panels, duplicate grants, duplicate stock, and population drift.
- Retain machine-readable soak results and the relevant logs/performance captures.

## Deep-review closure

- **Owner surfaces:** the Day 38 candidate entry point, packaged listen/dedicated clients/servers, persistence namespace creation, and the diagnostics metrics collector.
- **Required artifacts:** `day-39-soak.json` with lane, cycle, warm-up, duration, failure, peak, cleanup, and artifact-growth fields; per-lane logs and performance captures.
- **Gate:** each of the four role/topology lanes completes 10 full cycles within a 45-minute lane cap; no stage hangs beyond 120 seconds, no owned process remains after 30 seconds, and there are zero cross-run contamination, data loss, duplicate grant/stock, or stale-panel defects. A post-warm-up server/client memory increase above 15% or unexplained monotonic artifact growth blocks the candidate. An overnight soak is optional evidence, not a hidden prerequisite.

## Completion gate

The declared local/LAN soak threshold passes with no hangs, orphans, cross-run contamination, data loss, duplication, or unexplained candidate-blocking warning.

## Defer

Do not optimize crowds unless this measured fixture violates the declared budget.
