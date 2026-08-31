# Day 41 baseline evidence contract

Status: implementation contract; the schemas below do not claim that qualifying baseline artifacts already exist. Read with [Day 41](Day-41-Gameplay-Contract-and-Baseline.md) and the [shared execution contract](Execution-Contract.md). This contract adds a strict consumer audit; it does not rewrite the old scope, weaken Day 40, or change an existing package hash's meaning.

Implemented support-reader subformats and their limits are specified in [Baseline Support Reader Formats](Baseline-Support-Reader-Formats.md). Current remediation results are recorded in [the blocker remediation report](../../Reports/Gameplay-Baseline-Blocker-Remediation-2026-08-31.md). These additions do not declare a qualifying baseline or waive the six remaining semantic/provenance readers.

## Dispositions and scope phase

Keep three distinct results: `toolingStatus` (`PASS`, `FAIL`, `NOT_RUN`), `evidenceStatus` (`PASS`, `FAIL`, `BLOCKED`), and derived `runtimeEntry` (`READY`, `BLOCKED_RUNTIME_ENTRY`). A local test PASS never sets runtimeEntry directly.

The gameplay scope's `phase` is `Draft` or `Frozen`. Both require the proposed profile, feature inventory, nine lane definitions, seeds, authority contracts and finite numeric acceptance targets. In Draft, unavailable `sourceRevisionAtFreeze`, `baselineEvidence`, hardware, observations, trace and survey references are JSON null, with typed blocker entries; do not use plausible-looking filenames or fabricated measurements. Draft always derives BLOCKED_RUNTIME_ENTRY. Frozen requires a full source revision at freeze, all non-null evidence references, and successful validation of every required check below. A Frozen label with missing or failing evidence is invalid and remains blocked. The validator never silently freezes a Draft.

`sourceRevisionAtFreeze` records the existing audit HEAD when freezing is requested; it is not a prediction of the commit that will later contain the scope file. Verify it exists and is an ancestor of the current audit HEAD. Store scope byte hashes in results, not as a self-referential field inside the hashed scope. Later design changes create a different scope hash and invalidate dependent evidence.

An implementation that does not yet support any required check reports `VALIDATOR_CAPABILITY_MISSING` and cannot produce READY. There is no skip-validation or allow-synthetic production option. Unit-test fixtures may prove validator behavior but are never baseline execution evidence.

## Explicit inputs and roots

Implement these optional input switches on the Day 41 adapter and validator:

```powershell
./Scripts/ValidateGameplayExpansionScope.ps1 -ScopePath <scope.json> -BaselineEvidencePath <bundle.json> -EvidenceRoot <evidence-root> -PackageRoot <package-root> -AsJson
./RunGameplayExpansion.ps1 -Day 41 -Stage Preflight -RunId d41-audit -BaselineEvidencePath <bundle.json> -EvidenceRoot <evidence-root> -PackageRoot <package-root>
```

The omitted baseline input is permitted for Draft diagnostics and returns a named blocker. The default evidence root is the explicit bundle's directory. A package root must be explicitly supplied before package verification; neither a bundle nor a command searches for the latest package or successful report. `-ScopePath` defaults to the gameplay scope beside this contract. The adapter records every resolved input path and input hash.

A new file reference, called `FileRef` below, has exactly `{ "root": "evidence" | "package", "path": "relative/path", "sha256": "64 lowercase hex characters", "sizeBytes": 123 }`. `sizeBytes` is a non-negative JSON integer; substantive records, logs, captures and traces must be nonempty. Ordinary JSON records and observations use the evidence root; package inventory entries use the package root. The bundle itself and its legacy manifests must reside under the evidence root. The scope may use a repository-relative path to the bundle, resolved once by the adapter, outside the bundle's FileRef namespace.

Reject absolute/UNC/device paths in FileRef, `..` segments, drive prefixes, alternate data streams, wildcard paths, directory references, embedded NUL, empty segments and case-insensitive duplicate normalized paths. Resolve paths and require separator-aware containment under their selected root. Reject symlink/reparse-point components for this version, including a reparse root. Hash opened regular-file bytes and compare length; detect a file changing during hashing and fail. No evidence file is executed as code. Report paths in artifacts without credentials or raw player identities.

The old candidate stores path strings, some absolute. Preserve those bytes. Resolve its original path strings, require containment under the explicit evidence root, and require equality to the corresponding supplied FileRef target. Version 1 does not invent relocation mappings; an inaccessible original reference remains BLOCKED. References outside the root fail. Users can explicitly select the original common artifact root. No copying or editing an immutable old record to make a path check pass.

RunId accepts only `[A-Za-z0-9][A-Za-z0-9._-]{0,95}`, excluding `.` and `..`; require the exact value, never sanitize collisions into the same output directory. Use a fresh stage directory and fail if its output already exists.

## Audit bundle schema version 1

The bundle requires these fields. New schemas reject unknown fields unless placed under an optional `extensions` object; extensions never satisfy a required check. Reject duplicate JSON keys at every depth, invalid UTF-8, non-finite numbers, booleans in numeric fields and string-to-number/boolean coercion. Legacy records retain their existing field sets and may contain additional non-authoritative fields.

| Field | Type and meaning |
| --- | --- |
| `schemaVersion` | Integer 1. |
| `kind` | Exact `GameplayBaselineEvidence`. |
| `candidate`, `draft`, `laneEvidence`, `soak`, `finalFast` | Evidence FileRefs to the original Day 40 inputs/output and final fast result. |
| `legacyScope`, `legacyContentManifest` | Evidence FileRefs to the exact old manifest bytes. |
| `packageManifest`, `buildReceipt` | Evidence FileRefs defined below. |
| `operations`, `visual`, `humanReview` | Evidence FileRefs to the support/observation records defined below. |
| `hardware`, `baselineObservations`, `baselineTrace`, `anchorSurvey` | Evidence FileRefs for measured Day 41 inputs. `baselineObservations` is a JSON index referring to the Markdown observations; `baselineTrace` identifies the nonempty `.utrace` bytes. |
| `packageIdentity` | Object `{ "kind": "manifest-bytes-v1" | "archive-bytes-v1", "sha256": "...", "archive": null | FileRef }`; rules below. |

A missing bundle is a Draft blocker. A supplied bundle must be structurally valid; it cannot contain null records and call itself evidence complete. Partial collection belongs in `prerequisite-remediation.json`, whose `missing` list names the field, required evidence, reasonCode and next producer. It does not masquerade as a bundle.

Where a new record requires the `candidate binding fields`, the exact set is `runId`, `sourceRevision`, `scopeRevision`, `scopeManifestSha256`, `contentManifestSha256` and `packageSha256`, all equal to the verified candidate. A support record can add its own `collectionRunId` under extensions without changing the candidate RunId. Observations made during a later audit still identify the original immutable baseline package; their own collection timestamps remain actual timestamps.

## Preserve and verify the real Day 40 schema

The existing `RunPlayableCandidate.ps1 -Finalize` emits the following schema 2 fields. Require all of them with these checks; do not demand invented fields inside the immutable candidate.

| Existing field | Required check |
| --- | --- |
| `schemaVersion` | Integer 2. |
| `scopeRevision` | Exact `playable-candidate-v1`. |
| `sourceRevision` | Full 40-character lowercase Git commit ID for this repository. |
| `runId` | Safe, nonempty run ID. |
| `status`, `localDisposition` | Both exact `PASS`. |
| `externalDisposition` | `BLOCKED` or `PASS`; does not waive inherited local Shipping/operations requirements. The current finalizer writes BLOCKED. |
| `scopeManifestSha256`, `contentManifestSha256`, `packageSha256` | Valid SHA-256 values bound to verified inputs. |
| `draftPath`, `soakPath`, `laneEvidencePath`, `finalFastPath` | Original paths resolve to the corresponding bundle records. |
| `finalizedAtUtc` | Parseable UTC timestamp, no earlier than the completed Day 40 draft/lane/soak/operations/visual runs. Later Day 41 observations and review need not predate finalization. |
| `immutable` | JSON boolean true. |

Reject a schema 1 local-contract stub, a draft passed as a candidate, missing fields, diagnostic disposition or an explicit `validationMode: local-contract`. Existing old `candidate.json` files with only `passed: true` are not qualifying evidence. Record candidate byte hash and file attributes. Read-only attributes corroborate local immutability but do not replace content hashes or prove execution.

The draft must have schema 2, `stage: Candidate`, `mode: Both`, `role: Both`, `status: PASS`, `localDisposition: PASS`, `localContractStatus: PASS`, `packagedStatus: PASS`, empty `workingTreeStatus`, and no blockers. Its `runId`, `sourceRevision`, `scopeRevision`, scope/content manifest hashes and package hash must equal the candidate. The current Candidate runner cannot emit this successful packaged draft; implementing its producer is prerequisite remediation, not a reason to accept a Fast draft.

The legacy final-fast file has schema 1 fields `scopeRevision`, `days`, `mode`, `testsRun`, `failures`, `passed`, `durationSeconds`; require `days: all`, `mode: Both`, positive test count, empty failures, true passed and non-negative finite duration. Its source/run binding comes from the candidate's exact path, the bundle hash and the build/execution receipts, not nonexistent source fields. This is structural regression evidence only.

## Git and content provenance

1. Resolve candidate source with `git rev-parse --verify <source>^{commit}` and compare the full object ID. Require `git merge-base --is-ancestor <candidate-source> <audit-head>` exit 0; exit 1 is non-ancestry and other exits are Git failures.
2. Read the old scope and content-manifest blobs from that exact commit without checking out or modifying the working tree. Verify their bytes against the candidate hashes and supplied evidence copies. The current old scope must have the same byte hash; reject drift. The current old content manifest must also remain unchanged for this baseline tooling milestone.
3. Record current HEAD and full dirty-path list separately. Draft tooling may operate on dirty source but cannot pretend those paths were in a historical package. The build receipt below must capture clean committed source when the package was produced.
4. Preserve hashes of the old scope, content manifest and all supplied old artifacts before and after the audit. Any audit-side mutation fails `LEGACY_ARTIFACT_CHANGED`.

Do not bind historical Day 40 evidence to future Day 41 code. Day 60 separately reruns legacy regressions on final binaries as specified in the shared contract.

## Package identity and actual bytes

Package identity must be documented by the original build/package receipt; a 64-character string alone is insufficient. Support only these explicit meanings:

- `manifest-bytes-v1`: candidate `packageSha256` equals SHA-256 of the exact package-manifest file bytes. `archive` is null. The manifest does not contain its own byte hash. A remediation producer can choose this format for a new run before producing any evidence.
- `archive-bytes-v1`: candidate hash equals SHA-256 of the supplied archive bytes. `archive` is a package FileRef. The build receipt binds that archive to the manifest. Validate an archive inventory against actual extracted package entries using the declared supported archive format; a format without an implemented inventory reader yields BLOCKED, never an assumed match.

An old record with no provable package identity convention yields `PACKAGE_IDENTITY_UNPROVEN`. Never reinterpret its hash as a manifest hash, set it to an executable hash, or patch its immutable candidate. The separate package-manifest FileRef always hashes the manifest itself, whichever candidate identity convention was used.

The new package manifest has `schemaVersion: 1`, `kind: PlayableCandidatePackageInventory`, `sourceRevision`, `runId`, `scopeRevision`, `scopeManifestSha256`, `contentManifestSha256`, `engineVersion`, `createdAtUtc`, and `entries`. Each entry has `path`, `sizeBytes`, `sha256`, `kind` (`launcher`, `executable`, `library`, `container`, `content`, `runtime-resource`), `target` (`Aura`, `AuraServer`, `Shared`), and `configuration` (`Development`, `Shipping`, `Shared`). Entries are sorted by normalized path and unique; targets/configurations are verified against build receipts and actual launch receipts, not filename guesses.

Resolve `buildReceipt.packageSubdirectory` beneath PackageRoot with the same containment/no-reparse rules; it is a nonempty safe relative directory such as `extracted`, not an absolute path or `..`. Inventory entry paths are relative to that directory. Enumerate that entire immutable package subtree and compare inventory coverage and every file's byte hash/size. Require both client and server launcher plus inner executable, runtime libraries/resources, all `.pak`, `.utoc`, `.ucas` containers, staged loose configuration/XML/WebUI content, and StartupMap/content cook inventory. Absence of a file class must be explained by the actual packaging format and its validated manifest, not an empty category. Audit staged cook/container listings for the old content manifest's required paths and the discovered shipped JSON/XML/WebUI set; merely finding a filename in a source manifest is insufficient.

Runtime outputs must use external `-UserDir`/log/save directories and are not allowed to mutate the immutable package root. The inventory has no arbitrary exclusion globs; its own manifest lives in the evidence root. Archive identity files reside outside the extracted package inventory subtree, whose relative root is recorded by `buildReceipt.packageSubdirectory`. No executable, DLL, container, config or plugin path may be omitted as an exclusion. Unsupported container audit is a named capability blocker.

The new build receipt has `schemaVersion: 1`, `kind: PlayableCandidateBuildReceipt`, the candidate's binding fields (source/run/scope/content/package hashes), `packageIdentityKind`, `packageManifestSha256`, `packageSubdirectory`, `engineVersion`, `engineBuildVersion` (FileRef), `workingTreeStatusBefore`, `workingTreeStatusAfter` (both empty), `startedAtUtc`, `completedAtUtc`, and `commands`. Each command records `target`, `configuration`, redacted `arguments`, working-directory label, UTC bounds, monotonic duration, exit code and nonempty log FileRef. Require successful relevant build/cook/stage steps and clean-source capture contemporaneous with the run. Package entries must correspond to the produced outputs. A log or clean-tree boolean supplied afterward without provenance is not enough.

## Lane and observation receipts

New collected lane evidence preserves the fields the old finalizer consumes: schema version 2, candidate binding fields and `lanes`. Each lane has `laneId`, `role`, `topology`, `status`, `packageSha256`, `execution` and `checkpoints`. Require exactly these mappings:

| Lane | Role | Topology and process records |
| --- | --- | --- |
| Aura-listen | Aura | One packaged listen host and one remote client. |
| BungeeMan-listen | BungeeMan | One packaged listen host and one remote client. |
| Aura-dedicated | Aura | One packaged dedicated server and two remote clients. |
| BungeeMan-dedicated | BungeeMan | One packaged dedicated server and two remote clients. |

All four lane statuses must be PASS. `execution` contains `runId`, `laneId`, `cycleId` (null for the sign-off journey), `startedAtUtc`, `completedAtUtc`, `durationSeconds`, `worldIdHmac`, `namespaceId`, `processes`, `cleanup`, `warnings` and `evidence`. Process records contain role in topology, assigned player role where applicable, PID, parent/child ownership, package executable path/hash, redacted launch arguments, start/end UTC, exit code and log FileRef. Bootstrap and inner child ownership must be accounted for; process count alone is insufficient. Cleanup records duration, stopped owned PIDs, remaining owned PIDs, port-release observation and evidence; no remaining child or port leak is allowed.

Checkpoints have `id`, `status` (`PASS`, `FAIL`, `NotApplicable`), `startedAtUtc`, `completedAtUtc`, `durationSeconds`, `observations`, and at least one evidence FileRef for PASS. `observations` are typed producer records naming observed authority and client state before/after, observer process, correlation/session/generation and expected assertion. They must corroborate referenced logs/captures, not contain only `passed: true`.

Require all 16 unchanged old scope journey IDs plus `late-join`, `forced-kill-recovery` and `firearm-semantics`. BungeeMan firearm semantics include accepted shot consumption, held-input non-repeat, reload completion/cancellation and owner HUD result. Aura firearm semantics are NotApplicable with the role reason; Aura still proves attack/result. Validate meaningful player movement/target/attack, battle outcome, reward then spend, death/recovery, save commit, graceful restore and new-session replay. Direct health/wallet/objective-success mutation, an editor process or development-only probe cannot prove the public-input journey. Authority fixture setup is recorded separately before play.

Each receipt producer declares a `producerId` and `producerVersion` in its `extensions.producer` metadata and supplies its executable/script hash in execution evidence. The validator supports an explicit receipt-version reader and checkpoint assertion mapping. Unknown readers, absent mapping or unsupported observation semantics are capability blockers. Do not treat a plausible prose observation as automatically machine verified.

## Soak, operations, visual and human review

The extended soak record preserves old finalizer fields: schema 2, candidate binding fields, `status: PASS`, `passed: true`, `cyclesPerLane: 10`, and four `lanes`. Each lane contains its ID, ten unique cycles numbered 1 through 10, elapsed duration, normalized memory samples, artifact-growth samples and one additional forced-kill recovery execution. Every cycle uses the execution/checkpoint receipt shape above and proves battle, reward, purchase, death, recovery, disconnect, reconnect, save, graceful restart and restore. The aggregate number 10 does not substitute for cycle records.

Enforce lane duration <=2700 seconds, every stage <=120 seconds, cleanup <=30 seconds, and no incomplete cycle. Each cycle has a fresh namespace retained through that cycle's reconnect/restart; namespaces cannot collide across cycles/lanes/runs. For each server/client process role, compare median working set in declared equal post-warm-up and final windows; require positive baseline and growth <=15%. Capture sample timestamps and counts. Require zero cross-run contamination, data loss, duplicate reward/stock, stale UI, unexplained population/replay drift and blocking warning. Unexplained monotonic artifact growth blocks. Domain observations and growth explanations require the explicit review below if the machine reader cannot establish them.

All four following support records have `schemaVersion: 1`, distinct `kind`, candidate binding fields, `status`, UTC bounds, evidence FileRefs and supported producer metadata:

- `PlayableCandidateOperations`: exactly two clean start/ready/two-client/play/save/stop/restart/restore runs plus one forced-kill recovery, using execution receipts. Require authoritative readiness <=120 seconds, teardown <=30 seconds, preserved last committed values, `configuration: Shipping`, and second-developer attestation. Development/editor runs are diagnostic. An external BLOCKED label does not waive Shipping identity requirements inherited from Day 37.
- `PlayableCandidateVisual`: per-lane rendered captures for login/loading, role/HUD, attack, firearm or Aura NotApplicable, trade, death/recovery and reconnect. Include resolution, settings, renderer, capture UTC and originating process. Validate image decoding/nonzero dimensions and file integrity; human review establishes legibility/timing. NullRHI or browser HTML snapshots cannot prove rendered gameplay.
- `GameplayBaselineHumanReview`: pseudonymous implementer/reviewer IDs, distinct operator/environment IDs for the second-developer run, actual review UTC, explicit consent/authorized collection indication, reviewed record hashes, findings and `blockingFindings`. Hash coverage includes candidate, draft, lanes, soak, package/build, operations, visual, hardware, observations, trace and survey records; it excludes the review itself and the bundle to avoid self-reference. Required dispositions cover journey observation plausibility, visuals, privacy, economy/persistence/duplication, warning explanations, operations independence and baseline survey usability. A reviewer must inspect the records; the tooling cannot create this attestation on their behalf. Zero unresolved P0/P1 or other contract-blocking findings is required.
- `GameplayBaselineObservations`: both Aura and BungeeMan sessions with source/package binding, hardware reference, 1080p Medium settings, >=300-second warmup and >=300-second observation/trace window, observer pseudonym, raw measured times/shot and reload observations, objective clarity, damage causes and boredom notes; link the nonempty `baseline-observations.md` and trace. Notes can report an unknown measurement rather than invent one, but a missing required observation blocks freeze.

Machine-verifiable integrity means valid schemas, ancestry, actual bytes, matching bindings, supported log/result readers and arithmetic thresholds. It does not prove a person played, consented, reviewed honestly or used a genuinely separate environment. Human records are attributed attestations, never an automated PASS. Require their explicit presence and review dispositions in addition to machine checks; preserve this trust limitation in the audit result. No unsigned JSON format can defeat a malicious actor fabricating every log and attestation.

## Measured hardware, traces and mission space

`hardware` uses `schemaVersion: 1`, `kind: GameplayBaselineHardware`, `capturedAtUtc`, `machineId` (run-scoped pseudonym), OS/version, CPU model, physical RAM bytes, GPU model/VRAM, driver version, engine version, capture-command/log references and target display/render settings. Numbers are measured; scope frame/network/memory limits remain labeled acceptance targets. A machine descriptor extracted from a NullRHI automation report alone is not a rendered performance baseline.

`anchorSurvey` uses `schemaVersion: 1`, `kind: GameplayBaselineAnchorSurvey`, source/package/content binding, `canonicalMap: /Game/Maps/StartupMap`, `mapAlias: RoleBattleCivilianTest`, `surveyedAtUtc`, engine version, capsule dimensions, navigation configuration, geometry asset references/hashes, anchors, query evidence and captures. Require hub, two combat cells, escort start/shelter, relay pads, boss area and safe-zone exclusion observations for at least the first arrangement. An anchor records finite position/rotation, actual collision-fit query, nav projection/path result, connected-anchor IDs and safe-zone overlap result. Validate graph reachability and query outcomes with a supported engine survey reader. JSON coordinates or a hand-drawn diagram alone do not prove usable space. Day 49 supplies the second arrangement later.

Hardware, observations, trace and survey are bound to the same measured baseline package/source/content; the candidate source can precede the current documentation-only audit source. Trace/capture provenance names process and UTC window; require consistency with the observations. If a usable survey requires layout changes, create an explicit remediation dependency and rerun affected package/evidence checks; do not authorize Day 42 by inventing transforms.

## Fresh local tests and result output

Day 41 Preflight never launches a build/game. Fast additionally runs tooling tests, the old Fast regressions and a fresh bounded native `Aura.RoleBattle` suite using the verified engine's build and editor command tools. Import no prior result by recency. Give each process fresh run-owned logs/export directories and record executable hashes, command, source dirty state, start/end/monotonic times, exit and cleanup. Current diagnostic dirtiness is not a historical package cleanliness assertion.

Parse fresh UE `index.json`: nonempty unique `tests[].fullTestPath`, per-test `state`, errors/warnings, aggregate succeeded/succeededWithWarnings/failed/notRun/inProcess, and fresh log discovery/explicit zero completion marker. Aggregate counts must agree with per-test results and discovered count; no failed, pending or not-run required test. Reject report reuse, mismatched namespace, parse error, missing log/report, child nonzero exit or orphan. Classify warnings explicitly. Do not hardcode a successful count from an older log and assume current discovery matched it.

Emit `day-41.json` under the stage-owned directory within `Saved/Reports/GameplayExpansion/<audit-head>/<RunId>/`, plus `prerequisite-remediation.json` when blocked. Include the shared result fields, `phase`, separate tooling/evidence/runtimeEntry dispositions, verified input hashes, candidate source/ancestry result, current dirty paths, implemented/missing validator capabilities, each check's status/evidence/reasonCode and human-attestation limitations. Non-applicable package/test fields are null with a typed reason, never fabricated values.

Exit precedence: `3` for timeout/cleanup failure, then `1` for malformed/tampered/failed checks, then `2` for missing evidence/capabilities or Draft/runtime-entry blockage, otherwise `0`. Thus fully passing available local checks still return 2 while entry is blocked. Wrong stage/day combinations are argument failures. Suggested reason codes include `BASELINE_NOT_SUPPLIED`, `EVIDENCE_MISSING`, `LEGACY_STUB_REJECTED`, `EVIDENCE_HASH_MISMATCH`, `SOURCE_NOT_ANCESTOR`, `DIRTY_PACKAGE_SOURCE`, `PACKAGE_IDENTITY_UNPROVEN`, `PACKAGE_BYTES_MISSING`, `CHECKPOINT_EVIDENCE_INCOMPLETE`, `SOAK_CYCLES_INCOMPLETE`, `SHIPPING_OPERATIONS_MISSING`, `HUMAN_REVIEW_MISSING`, `ANCHOR_SURVEY_UNVERIFIED`, `VALIDATOR_CAPABILITY_MISSING` and `DRAFT_SCOPE`.

Required behavioral tests use temporary files/Git repositories and real hash mutations: schema-1/draft rejection; duplicate JSON keys and unsafe roots; missing/tampered/partial package inventory; exact ancestor/non-ancestor cases; dirty build receipt; missing/duplicate lane; aggregate-only soak; absent public checkpoint/Shipping operations/human review; Draft never ready; Frozen missing survey; unsupported reader; stale/empty/failed native export; old artifacts unchanged. A positive synthetic fixture proves only validator logic. Runtime entry still needs an actual independently reviewed baseline bundle.
