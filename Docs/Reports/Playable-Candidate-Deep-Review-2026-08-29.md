# Playable Candidate Plan — Deep Review

Date: 2026-08-29  
Scope: Days 21–40 and the master Playable Candidate plan  
Review mode: local/legacy fallback after bounded in-app handoff failure

## Review status

The in-app ChatGPT handoff was attempted with a scoped packet containing only the Days 21–40 plan summaries, dependencies, proposed implementation surfaces, validation gates, and structural concerns. The first bounded attempt timed out while opening the page. The second timed out while entering the packet; page inspection confirmed that the packet remained unsent in the composer. No external response was received, so this report does not claim that ChatGPT reviewed the plans. Per the handoff skill's continuity fallback, the review below uses the local repository, all 20 generated daily contracts, the master plan, the completed Role/Battle plan references, and the existing runner/configuration surfaces.

## Executive verdict

The plan has the right milestone shape, but its first draft was a good roadmap rather than an implementation-grade contract. The material risk was not missing feature ideas; it was leaving foundational decisions open until the days that depend on them. The revised plan is implementation-grade for a local/LAN Playable Candidate because it now freezes the firearm, persistence, reward, restock, identity-privacy, ownership, artifact, and release-gate decisions before feature work begins.

The public authenticated release remains a separate external gate. Missing production provider/App ID provisioning and two stable authorized identities must be reported as `External/Public Multiplayer = BLOCKED`; they cannot fail or falsely pass the local/LAN candidate.

## Ranked findings and dispositions

### P0 — must be closed before feature implementation

1. **Foundational decisions were deferred.** Day 27 said “normally semi-auto,” Day 30 left time/offline behavior open, and Day 31 made ammo/tutorial persistence conditional. These are contract inputs, not implementation details. Day 21 now freezes semi-auto press semantics, player-state persistence, server time, capped offline restock, and forced-kill rollback.
2. **The evidence pipeline had no owned entry point.** Day 38 required “one entry point” without naming it or defining status propagation. It now names `RunPlayableCandidate.ps1`, its three stages, per-stage results, bounded child ownership, and nonzero propagation for every failed stage.
3. **Local and public release gates could be conflated.** Day 40 now defines four mandatory local lanes and an independent external status. An unavailable provider is not a skipped test and cannot contaminate the local candidate disposition.
4. **Cross-system mutations needed an atomic boundary.** Day 29 now has one explicit reward definition and transaction/correlation revisions. Day 30 and Day 31 bind merchant stock, restock time, wallet, inventory, ammo, tutorial progress, and recovery state to explicit player/world owners and commit generations.

### P1 — must be explicit before the relevant day closes

1. Every day now names an owner surface, a required artifact, and a machine-readable gate. Planned paths are labeled as planned outputs rather than being presented as existing files.
2. Day 22, 34, 35, 36, and 37 now have failure thresholds and propagation rules. A warning, partial aggregate, stale log, or missing artifact cannot become a PASS.
3. Diagnostics now use a run-scoped HMAC identity representation and provider type. The key is supplied only to cooperating processes and is never logged or written to the candidate artifact, preventing cross-run linkage; raw external identifiers and secrets are prohibited.
4. Day 39 is narrowed from an ambiguous “multi-hour” requirement to four bounded lanes, ten complete cycles per lane, a 45-minute lane cap, 120-second stage timeout, and 30-second cleanup timeout. An overnight soak is optional evidence rather than a hidden blocker.
5. Historical map/runtime-fixture drift is handled as a manifest decision. The plan continues to use `/Game/Maps/StartupMap` and runtime-created AI/markers instead of creating duplicate assets.
6. The GSM plan is now executable: loopback remains the default; non-loopback startup fails closed without request/readiness tokens and an IP/CIDR allowlist; readiness cannot spoof a level port or client endpoint.
7. Candidate finalization now consumes explicit draft/soak paths and verifies `RunId`, scope hash, source revision, and package/content hashes. It never selects a “latest” artifact, and changed evidence must be regenerated.

### P2 — deliberately kept small

1. Visual QA uses a deterministic three-profile checklist rather than a screenshot-diff platform.
2. Fire mode remains semi-auto; automatic fire, durability, attachments, and a generalized weapon inventory are deferred.
3. Merchant work is fixed stock plus one restock interval. Schedules, employees, offline progression, and broad business simulation remain out of scope.

## Corrected dependency graph

```text
Day 21: scope, decisions, four-lane manifest
  |
  +--> Day 22: fresh post-tooling baseline
  |      |
  |      +--> Day 23: bounded boot/readiness/error
  |             |
  |             +--> Day 24: first-use contract
  |
  +--> Day 25: authoritative ammo
           |
           +--> Day 26: HUD state/replay
                  |
                  +--> Day 27: fixed semi-auto cadence
                  |
                  +--> Day 28: player death/recovery
                  |
                  +--> Day 29: explicit reward -> wallet/inventory -> merchant
                         |
                         +--> Day 30: deterministic stock/restock clock
                                |
                                +--> Day 31: versioned player/world persistence
                                       |
                                       +--> Day 32: late join/reconnect
                                              |
                                              +--> Day 33: negative-path/network hardening

Day 21 + Days 25–30 --> Day 34: canonical content/config manifest and validator
Day 22 + Day 33    --> Day 35: diagnostics and redaction
Days 23–28 + Day 32 --> Day 36: packaged visual QA
Day 31 + Day 35    --> Day 37: dedicated-server operations
Days 22, 34–37     --> Day 38: RunPlayableCandidate.ps1 and candidate artifact
Day 38             --> Day 39: bounded four-lane soak
Days 21–39         --> Day 40: independent local and external sign-off
```

The day files retain their original dependency declarations where the dependency is already transitive, but their closure sections make the required direct contract and artifact explicit.

## Day-by-day closure review

| Day | Required correction | Owner/output anchor | Exit condition |
| --- | --- | --- | --- |
| 21 | Turn scope discovery into a freeze gate. | `playable-candidate-scope.json`, existing StartupMap/role config, four lanes. | Manifest validates with no unresolved decisions. |
| 22 | Re-prove tooling before feature work. | Existing build/review/persistence/multiplayer runners; baseline JSON. | All selected commands exit zero; no orphan, reused log, timeout, or false success. |
| 23 | Bound every boot stage. | GSM plus login/loading/HUD WebUI pages; boot matrix JSON. | HUD-ready within 60 seconds or stage error within 30; 120-second hard timeout. |
| 24 | Give onboarding a versioned source and reset semantics. | Planned tutorial config plus existing native authorities; fresh/reset report. | Six steps complete without developer instructions; reset/reconnect are server-validated. |
| 25 | Make ammo a server-owned player state on the active XML path. | FireGun XML/role config, ability boundary, ammo contract. | Exactly one server-consumed round per valid press; no negative/forged/owner-leaked ammo. |
| 26 | Treat HUD replay as a state contract. | Three existing HUD pages plus WebUI bridge; HUD JSON/captures. | All combat/death/reconnect states replay once and remain readable/interactable. |
| 27 | Remove the fire-mode design ambiguity. | FireGun data and server input acceptance; fire-mode contract. | 100 hostile requests cannot exceed configured cadence; held input does not auto-repeat. |
| 28 | Separate player death from Civilian death. | Life-state, ASC/grant ledger, pawn replacement, HUD replay. | Three cycles per role/topology have one death and one recovery with no stale state. |
| 29 | Add only one explicit reward path. | Economy components, merchant config, authoritative combat outcome. | Reward is at-most-once and purchase is atomic/idempotent; client values are ignored. |
| 30 | Freeze deterministic restock semantics. | Merchant/world record; injected test clock. | No duplication/rollback; backward clock clamps; offline catch-up is capped at one interval. |
| 31 | Close the persistence contract rather than discover it. | Persistence subsystem/manifest plus player/world ownership table. | Committed values restore; forced kill rolls back; corrupt/unknown data fails closed. |
| 32 | Validate state convergence and privacy. | Replication/reconciliation/WebUI replay. | Late join ≤60 seconds, reconnect ≤90 seconds, idempotent replay, private state isolated. |
| 33 | Make rejection behavior observable and non-mutating. | New RPC/command table and Shipping compile guards. | Every negative case returns a code ≤2 seconds with zero partial mutation. |
| 34 | Use one exhaustive validator/manifest. | Planned validation script and canonical content manifest. | Clean set passes; malformed/missing fixtures exit nonzero; pipeline publication is blocked. |
| 35 | Make diagnostics useful without leaking identity. | Shared result writers, GSM, supervisors, artifact collector. | Nine synthetic failures are reconstructable; redaction test passes. |
| 36 | Test packaged visuals at declared profiles. | Shipping WebUI/layout/input boundary. | 1280x720/100%, 1920x1080/100%, and 2560x1440/125% have zero known blocking defects. |
| 37 | Make dedicated operation reproducible by another developer. | Existing dedicated batch scripts, GSM, planned runbook. | Two clean runs plus forced-kill recovery; readiness ≤120 seconds; cleanup ≤30 seconds. |
| 38 | Name and own the candidate pipeline. | `RunPlayableCandidate.ps1`, stage results, candidate JSON. | Requested stage fails nonzero on injected failure; no partial artifact is PASS. |
| 39 | Bound the soak and separate optional evidence. | Packaged four-lane runner and metrics collector. | 10 cycles/lane within 45 minutes; no lifecycle/data/privacy defect; memory growth ≤15%. |
| 40 | Publish two independent dispositions. | Immutable candidate JSON/report plus all supporting evidence. | Local four-lane matrix passes independently; public gate is PASS only with provider/accounts. |

## Evidence and failure rules

The authoritative chain is:

`revision → build configuration → canonical content manifest → test result → package hash → explicit draft/soak inputs → logs → visual evidence → candidate disposition`

The following are candidate-blocking failures: a missing or mismatched manifest, any nonzero required stage, an owned child that survives teardown, a timeout without an actionable stage result, false-success aggregation, shared persistence namespace, cross-owner private-state exposure, duplicate/partial economy or grant mutation, malformed staged content, or an unexplained P0/P1 warning. An external provisioning failure is classified only on the external row.

## Anti-goals confirmed

Do not add role hot-swapping, automatic fire, weapon durability/attachments, a generalized weapon inventory, full reputation/crime, rich schedules, broad business simulation, offline progression, unmeasured crowd optimization, wholesale legacy GAS/Blueprint cleanup, duplicate fixture maps/assets, cloud orchestration, a new test framework, or screenshot-diff automation during Days 21–40.

## Files revised by this review

- [Master implementation plan](../Plans/Playable-Candidate-Implementation-Plan-2026-08-29.md)
- [Day 21–40 implementation contracts](../Plans/Playable-Candidate-Implementation/)
- [Readiness review](Playable-Candidate-Readiness-Review-2026-08-29.md)
