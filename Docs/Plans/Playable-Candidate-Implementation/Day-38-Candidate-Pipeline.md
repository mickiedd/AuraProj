# Day 38 — Repeatable Candidate Pipeline

Status: Planned  
Depends on: Days 22, 34, 35, 36, and 37

## Goal

Create one entry point that produces a self-describing candidate artifact from a recorded revision.

## Work

- Split the pipeline into fast, candidate, and external release layers so missing production credentials do not block ordinary development.
- Implement the single checked-in entry point `RunPlayableCandidate.ps1` with `-Stage Fast`, `-Stage Candidate`, and `-Stage External`; each stage owns its children, writes a stage result, and propagates nonzero status.
- Orchestrate builds, content validation, focused/full automation, packaging, staged-data audit, selected packaged smoke, visual evidence collection, and hash generation through that entry point.
- Preserve bounded timeouts, child-process containment, explicit exit propagation, unique persistence namespaces, deterministic prerequisites, and exhaustive content discovery.
- Publish a machine-readable summary containing revision, configurations, manifests, results, package hashes, logs, and visual evidence paths.

## Detailed execution contract

### Files to inspect or create

- **Single entry point:** new checked-in root `RunPlayableCandidate.ps1` with `-Stage Fast|Candidate|External`, `-Revision`, `-Mode Listen|Dedicated|Both`, `-Role Aura|BungeeMan|Both`, and `-RunId` parameters.
- **Wrapped tools:** `build_test.bat`, `BuildDedicatedServer.bat`, `build.ps1`, `Scripts/ValidatePlayableCandidateContent.ps1`, existing review/persistence/multiplayer runners, WebUI/graph/AutoTest checks, Day 36 visual runner, and Day 37 server-ops runner.
- **New output:** `Saved/Reports/PlayableCandidate/<Revision>/<RunId>/candidate.json`, `stage-*.json`, package hashes, manifest hash, logs, visual paths, and `Docs/Reports/Playable-Candidate-<Revision>-<RunId>.md`.

### Pipeline contract

`Fast` validates scope/content and runs focused checks. `Candidate` builds/packages, audits staged content, launches the four local lanes, runs required visual/server/soak prerequisites, and writes the local disposition. `External` runs only when provider/App ID and two authorized identities are available; otherwise it writes `BLOCKED` with a provisioning reason and never changes the local disposition. Every stage has a stable result code, start/end time, child list, timeout, and exit status.

### Detailed steps

1. Parse parameters and manifest, resolve the requested revision, create a unique artifact root, and reject unknown stage/mode/role values.
2. Run the fast validator and focused checks; stop before build/package if content or prerequisites fail.
3. Build required Editor/Game/Server configurations with owned child processes and bounded timeouts; record binary hashes.
4. Run native/Python/graph/AutoTest suites and existing network/persistence checks; aggregate only machine-readable success results.
5. Package local listen/dedicated outputs, run staged-data/XML/JSON/HTML audits, and record package hashes.
6. Invoke packaged visual QA, server operations, and the four-lane candidate matrix; retain logs and captures by lane.
7. Run failure-injection checks for build, validator, test, package, process, manifest, and visual stages; prove each returns nonzero and no partial artifact is labeled PASS.
8. Emit `candidate.json` with evidence chain, stage results, hashes, paths, thresholds, known limitations, and separate local/external dispositions.

### Named automation and gate

- The root command is the only candidate publication entry point; day-specific runners remain independently usable for diagnosis.
- Required failure propagation: a failed child, timeout, crash, missing artifact, invalid manifest, nonzero test, package mismatch, or visual failure returns nonzero from the requested stage and from the root command.
- Missing external credentials are `External=BLOCKED`; they are not a Fast/Candidate failure and are never silently skipped.

## Validation and evidence

- Run a clean success pipeline and an injected failure at every major stage.
- Confirm the pipeline returns nonzero for build, validator, test, package, process, manifest, or visual-gate failure.
- Verify candidate artifacts are reproducible enough to answer “what exactly was tested?” without opening Markdown first.

## Deep-review closure

- **Owner surfaces:** new root `RunPlayableCandidate.ps1`, the existing build/smoke/package scripts, `Scripts/ValidatePlayableCandidateContent.ps1`, and the Day 34 canonical manifest.
- **Required artifacts:** `Saved/Reports/PlayableCandidate/<Revision>/<RunId>/candidate.json`, per-stage result files, package hashes, manifest hash, logs, visual paths, and a documented exit-code table. Missing external credentials produce `External=BLOCKED`, not a failed local candidate.
- **Gate:** one command runs each requested stage with a bounded timeout; injected build, validator, test, package, process, manifest, and visual failures all return nonzero and produce a precise stage/result code; no partial artifact is labeled PASS.

## Completion gate

One documented command produces a bounded, auditable local/LAN candidate or fails with a precise stage/result code.
