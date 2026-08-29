# Day 40 — Playable Candidate Sign-off

Status: Planned  
Depends on: Days 21–39

## Goal

Publish an honest, immutable decision for the local/LAN Playable Candidate and the separate public authenticated release gate.

## Work

- Run the complete supported local/LAN matrix for Aura and BungeeMan: boot, movement, targeting, attack, ammo/reload, Civilian/battle state, Interact/Trade, reward/spend, death/recovery, save, late join, reconnect, and server restart.
- Verify fast-gate, candidate-gate, packaged visual QA, server-operations, soak, and documentation artifacts are present and internally consistent.
- If production provider/App ID and two authorized accounts are available, run the authenticated packaged Shipping listen/dedicated two-remote-client matrix through the public RPC surface.
- Freeze known limitations and update the master status/index/reference docs without representing blocked rows as skipped or passed.

## Detailed execution contract

### Files to inspect or modify

- **Inputs:** Day 21 scope manifest, Day 22 baseline, Day 34 content manifest, Day 35 diagnostics packet, Day 36 visual rows, Day 37 operations run, Day 38 `candidate.json`, and Day 39 soak result.
- **Sign-off owners:** `Docs/Reports/Playable-Candidate-Deep-Review-2026-08-29.md`, the candidate report writer, `Docs/README.md`, the master plan, and the plan/reference indexes.
- **New output:** immutable `Saved/Reports/PlayableCandidate/<Revision>/<RunId>/candidate.json`, `Docs/Reports/Playable-Candidate-<Revision>-<RunId>.md`, known-limitations list, and final gate table.

### Sign-off matrix

The local matrix has four rows: Aura/listen, BungeeMan/listen, Aura/dedicated, and BungeeMan/dedicated. Each row must cover launch/login/loading, role/HUD, move/target/attack, ammo/reload/fire, Civilian/battle result, Interact/Trade, reward/spend, death/recovery, save, late join, reconnect, and restart where applicable. The report must link the exact logs, captures, package hash, content-manifest hash, and machine-readable result for every row.

### Detailed steps

1. Verify the candidate revision and scope manifest have not changed since Day 21; a change creates a new candidate run.
2. Check Day 22–39 prerequisites for complete artifacts, matching revision/package/manifest hashes, nonzero propagation, and no unresolved blocking warnings.
3. Run the complete four-lane local matrix from a clean packaged candidate and compare observed state to the frozen player/world contract.
4. Verify privacy, authority, atomic economy, persistence/recovery, late join/reconnect, visual, server-operations, and bounded-soak results.
5. Run the external provider/account matrix only when the preflight is `READY`; record authenticated identities as redacted stable references and keep its result on a separate row.
6. Classify every known limitation as non-blocking only if it does not affect the supported journey, security, privacy, persistence, duplication, content, packaging, or support gates.
7. Publish the immutable machine artifact and human report, update indexes, and record the exact local/external disposition.
8. Do not start the next feature wave until the local PASS and external BLOCKED/PASS states are explicit.

### Completion thresholds

- Local PASS requires all four lanes, all required journey checkpoints, Day 39 soak, Day 37 second-developer operations, zero P0/P1 defects, zero unexplained blocking warnings, zero data/duplication/privacy violations, and no timeout/cleanup failure.
- External PASS requires production provider/App ID, two distinct stable identities, packaged Shipping listen and dedicated two-remote-client matrices, and the same authority/privacy/persistence gates. Missing provisioning is `BLOCKED`, not skipped.

## Deep-review closure

- **Owner surfaces:** the Day 21 scope manifest, Day 38 `RunPlayableCandidate.ps1` artifact, Day 39 soak results, packaged launch/runbook evidence, and the Docs plan/report index.
- **Required artifacts:** one immutable `candidate.json` plus the human report, revision/build/package hashes, canonical content manifest, all four lane dispositions, visual captures, server/client logs, soak metrics, known limitations, and the separate external preflight result.
- **Gate:** `Local/LAN Playable Candidate = PASS` requires all four mandatory lanes, the player journey, privacy checks, persistence/recovery, Day 39 bounded soak, and second-developer operations gate to pass with no P0/P1 defect or unexplained blocking warning. `External/Public Multiplayer` is independently `PASS` only after production provider/App ID, two distinct stable identities, and both packaged Shipping matrices pass; otherwise it is explicitly `BLOCKED`.

## Validation and evidence

- Publish one candidate report with revision/build hashes, content manifest, test summaries, package hashes, server/client logs, visual captures, soak metrics, and explicit gate dispositions.
- Mark **Local/LAN Playable Candidate = PASS** only when its full matrix passes.
- Mark **External/Public Multiplayer = PASS** only after distinct stable provider identities and both packaged Shipping matrices pass; otherwise mark it **BLOCKED** with the provisioning reason.

## Completion gate

A person who has not opened the source tree can launch the candidate, understand the loop, play it through recovery and reconnect, and provide evidence that lets engineering diagnose a failure.

## Defer after sign-off

Only after the authenticated release gate passes should the next broad feature wave begin. Keep role hot-swapping, durability, full reputation/crime, rich schedules, broad crowd optimization, wholesale legacy cleanup, and cloud orchestration outside this milestone.
