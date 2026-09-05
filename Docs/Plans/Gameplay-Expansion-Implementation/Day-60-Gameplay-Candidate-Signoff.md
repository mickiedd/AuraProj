# Day 60 — Gameplay Candidate Sign-off

Status: Fail-closed finalizer and synthetic evidence-handling tests implemented. Candidate publication remains BLOCKED until the real packaged matrix, soak, performance, second-author, and human gates are supplied.
Depends on: Days 41–59; exact final technical, usability, performance, legacy regression and soak artifacts.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Publish a reproducible decision about whether the gameplay slice is stable, understandable and worth replaying.

## Exact change surfaces

- Extend `RunGameplayExpansion.ps1`, `Scripts/gameplay_expansion.py` finalizer/result validators and `Scripts/test_gameplay_expansion.py --day 60` using the shared explicit-input interface; retain old `RunPlayableCandidate.ps1` semantics. Reuse actual Day 42–59 native gameplay tests; no Day 60 C++ test file or native namespace is required for artifact aggregation.
- Inputs: new scope/manifest, all daily artifacts, final package manifest, Day59 playtest, Day58 performance, nine-lane gameplay matrix and old four-lane regression records.
- New: immutable `Saved/Reports/GameplayExpansion/<revision>/<run-id>/gameplay-candidate.json`, human report, known limitations, documentation index updates and final visual archive.

## Data and authority contract

No 'latest' artifact selection. Exact final source must be committed/clean; gameplay records bind source/scope/content/package hashes. Old Day40 prerequisite is ancestor provenance, while fresh legacy regressions execute final binaries under unchanged old scope. Dispositions are separate: Technical, Usability, Performance, ExternalProvider. LocalGameplay PASS requires the first three PASS and no P0/P1, duplication, privacy, persistence or unbounded-process defect. External may remain BLOCKED with a provisioning reason; it never becomes PASS from local fixture accounts.

## Numbered implementation steps

1. Freeze candidate build/content and verify explicit input files, schema/checksums and expected row sets. Any code/content change since testing invalidates affected evidence; no stale human/performance result is relabeled current.
2. Run the full 54 Standard +12 mutator matrix from Day55 and nine-lane lifecycle cases. Cover death/rescue, verified possession before Alive, choice, zero-resource closed boss arena, final settlement, ineligible-member marker cleanup, late entrant, targetable disconnect proxy/cooldown retention, expiry followed by later hub mutation, member-only forfeit, gameplay guidance merge/reset, graceful shutdown and forced kill.
3. Run old four-role/topology candidate regression on final package. Retain distinct scope/profile identities and owner privacy assertions.
4. Execute ten sequential complete runs per topology family (Solo, Listen, Dedicated), alternating roles/templates/layouts within the 240-minute per-family cap; record 30 cycles, hub cleanup, memory and settlement counts.
5. Validate final Day59 human thresholds from the fresh initial-session cohort and voluntary decisions recorded before scheduled comparisons; reject learned/repeated or stale-content first-use evidence. Check the second-author content gate and Day58 final-candidate performance evidence, retaining the separately identified matched pre-optimization baseline only as provenance. Classify missing external provider independently; do not send account credentials into reports.
6. Finalize once, publish known limitations, reproducible runbook/command lines, content inventory and archived visual. If any required local gate fails, publish a non-PASS review report and route the defect to its owner day.

## Named tests and commands

Python tooling file: `Scripts/test_gameplay_expansion.py --day 60`; test the real finalizer against temporary artifact trees, exact file hashes and malformed/missing/duplicate records. These validate evidence handling, never synthesize the gameplay, performance or human observations being validated. Native regression is the existing `Aura.Gameplay.Day42` through `Aura.Gameplay.Day59` groups plus relevant `Aura.RoleBattle` cases; require nonempty exported results for each expected group, not a nonexistent Day 60 namespace.

- FinalArtifactExactBinding — wrong revision/scope/content/package or dirty source rejects finalization.
- RequiredMatrixComplete — duplicates/missing 54+12 rows or missing lifecycle/legacy lanes reject.
- NoFalsePlayabilityPass — absent playtest/failed enjoyment/performance prevents LocalGameplay PASS.
- HumanEvidenceIsUncoercedAndCurrent — prior-exposure cohort, scheduled replay counted as voluntary, omitted unfavorable observations or invalidated first-use content prevents Usability PASS.
- FinalMetricsDoNotBorrowBaseline — pre-optimization/legacy samples remain comparison provenance and cannot substitute for exact candidate hardware/trace measurements.
- ImmutableFinalRecord — second write to the same RunId refuses without overwriting evidence.
- SoakCycleAndCleanupAccounting — exactly recorded cycles with zero reward duplication/orphan growth.
- ExternalGateIndependent — absent real-provider proof remains BLOCKED even with passing local lanes.

Planned runner commands (implement the adapter before use):

```powershell
python Scripts/test_gameplay_expansion.py --day 60 --output Saved/Reports/GameplayExpansion/day60-tooling.json
./RunGameplayExpansion.ps1 -Day 60 -Stage Fast -RunId d60-fast
./RunGameplayExpansion.ps1 -Day 60 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d60-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast runs Python finalizer behavior tests, content validation and actual native Day 42–59 plus relevant `Aura.RoleBattle` regressions, parsing exported results and rejecting missing groups. It never requests `Aura.Gameplay.Day60`. Packaged executes the complete matrix and lifecycle scenarios through actual game processes with rendered evidence; passing artifact fixtures cannot complete this day.

## Fixtures and topologies

FinalGameplayCandidate: full shared matrix, negative network/restart overlay, 30-cycle soak and final human cohort. Runtime can exceed a workday across batches; use bounded per-stage jobs and explicit progress, never reduce coverage to fit a calendar label.

## Failure and timeout semantics

Missing evidence BLOCKED; behavioral failures FAIL; human failures NEEDS_ITERATION; timeout/cleanup nonzero. Failed finalization leaves input artifacts intact and writes no PASS candidate. A repaired build requires fresh RunId and affected retests; never edit an old archive to conceal failure. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-60.json, technical.json, gameplay-playtest.json, performance.json, soak.json, regression.json, package manifest, immutable gameplay-candidate.json only after gate verification, human decision/limitations report and final SVG. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

A new player can choose a mission, use counterplay/builds, cooperate/recover, finish or fail fairly, receive durable results and voluntarily replay; this claim is backed separately by technical, measured performance and human evidence.

## Defer / anti-goals

No public-production release without its own authenticated proof, new feature work during sign-off, silent waiver of usability gates or artificial promotion of blocked rows.
