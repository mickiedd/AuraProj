# Baseline support reader formats

These are the implemented version-1 subformats for the [baseline evidence contract](Baseline-Evidence-Contract.md). They do not complete Day 41 or replace public-input, cook, Shipping, soak, or engine-survey verification. No receipt is generated on a person's behalf.

## Common support receipt

`PlayableCandidateVisual` and `GameplayBaselineHumanReview` require integer `schemaVersion: 1`, exact `kind`, all six candidate binding fields, `status: PASS`, `startedAtUtc`, `completedAtUtc`, nonempty `evidence` FileRefs, and `extensions.producer`:

```json
{
  "producerId": "AuraBaselineReceipt",
  "producerVersion": 1,
  "source": {
    "root": "evidence",
    "path": "collector-source.py",
    "sha256": "<actual 64-character lowercase SHA-256>",
    "sizeBytes": 123
  }
}
```

This example is a format illustration, not usable evidence. `source` identifies the actual collector/review tool bytes for inspection; those bytes are read and hashed, never executed by the reader. This is a receipt-format identifier, not a claim that a collector, signer, game driver, or trusted identity provider is implemented. Unknown producer IDs/versions return `UNSUPPORTED_EVIDENCE_PRODUCER` / BLOCKED. New records reject extra authoritative fields; optional `extensions` must be objects and cannot satisfy required fields. UTC intervals cannot run backward. If a duration is supplied to the interval helper it must be finite, nonnegative, and agree with UTC elapsed time within one second.

Every transitive reference, including images, review notes, collector source and finding-resolution evidence, receives the same root, regular-file, no-reparse, length and SHA-256 checks as top-level references. Verified transitive hashes and all inventoried package files are checked again before audit completion. A later mutation fails `LEGACY_ARTIFACT_CHANGED`; the reader never edits the inputs. Image/record parsing retains the 64 MiB file limit; package and archive hashes stream in bounded chunks.

## ZIP package identity

`manifest-bytes-v1` requires no archive and keeps its existing identity meaning. `archive-bytes-v1` additionally requires `buildReceipt.extensions.archiveFormat: zip-v1`, an archive FileRef using the explicit package root, and actual archive bytes matching the candidate identity. The archive must be outside `packageSubdirectory`.

ZIP v1 supports stored and deflate members. It reads members without extraction and compares every size and SHA-256 to the already verified extracted-package inventory. Member paths must use safe relative forward-slash paths. Duplicate/case-aliased names, traversal, NUL-truncated names, symlinks/devices, file/directory collisions, missing/extra files, unlisted empty directories, CRC/decompression errors and changing bytes fail. Encrypted members or unsupported archive/compression formats remain BLOCKED. Metadata reads are bounded to 64 MiB before the ZIP parser allocates its directory records. There is a 200,000-member limit and a 120-second decompression audit deadline; the adapter also bounds the owning audit process.

This verifies archive identity and coverage. Build provenance and inspecting the contents of `.pak` / `.utoc` / `.ucas` are separate, still-blocking capabilities; a ZIP containing those files does not prove their cooked contents.

## Rendered captures

The visual record adds `captures`, with exactly one entry for each combination of the four old lanes and these seven states: `login-loading`, `role-hud`, `attack`, `firearm`, `trade`, `death-recovery`, `reconnect`.

Each capture requires `laneId`, `state`, and `status`. Aura's two firearm rows must use `NotApplicable` and exact reason `Aura has no firearm`, with no alleged image data. The other 26 rows require:

| Field | Check |
| --- | --- |
| `image` | Evidence FileRef; unique frame reference, decoded PNG/JPEG. |
| `resolution` | Exactly two positive JSON integers; equal decoded dimensions. |
| `quality` | `Low`, `Medium`, `High` or `Epic`. |
| `renderer` | `D3D11`, `D3D12` or `Vulkan`; no NullRHI. |
| `capturedAtUtc` | Inside collection and originating-process intervals. |
| `processId` | Positive integer; matches exactly one `listen-host` or `client` process in that lane's execution receipt. |

Process records supply integer `pid`, `role`, string-array `arguments`, and UTC start/end. NullRHI launch arguments are rejected, including assignment forms. Pillow verifies and fully decodes the frame, rejects animation and corrupt image data, and limits dimensions to 8192 per axis and 33,554,432 total pixels. Missing Pillow is `IMAGE_DECODER_MISSING` / BLOCKED, not PASS. Tests exercise actual image decoding; they are explicitly synthetic fixtures.

Image decoding and declared process binding do not prove gameplay occurred or establish legibility. The still-required execution/provenance reader and human dispositions provide those additional gates. A renamed browser screenshot or fabricated capture metadata is not rendered gameplay evidence.

## Attributed human review

The human record adds `implementerId`, `reviewerId`, `implementerEnvironmentId`, `operatorId`, `operatorEnvironmentId`, `authorizedCollection`, `reviewedAtUtc`, `reviewedHashes`, `dispositions`, `findings`, and `blockingFindings`.

IDs are safe run-scoped pseudonyms, never credentials or raw provider IDs. Reviewer and operator must differ from the implementer; the operator environment must differ from the implementer's environment. Operator/environment must match the operations record. `authorizedCollection` must be JSON true, not a string or number. Review time must fall inside its collection interval and not predate the candidate or any supplied support-record completion/capture/survey timestamps.

`reviewedHashes` contains exactly the actual hashes of candidate, draft, laneEvidence, soak, packageManifest, buildReceipt, operations, visual, hardware, baselineObservations, baselineTrace and anchorSurvey. It excludes the review and bundle to avoid self-reference. `dispositions` has exactly `journey`, `visuals`, `privacy`, `economyPersistence`, `warnings`, `operationsIndependence`, `baselineSurvey`, all PASS.

`blockingFindings` must be empty. Each `findings` entry requires a unique nonempty `id`, `severity` in P0–P3, `status` Open/Resolved, boolean `contractBlocking`, and nonempty `summary`. Open P0/P1 or contract-blocking findings reject acceptance even when the top-level blocking list is empty. Resolved findings require nonempty `resolutionEvidence` FileRefs with verified bytes.

The machine verifies consistency of an attributed attestation. It cannot establish that a person actually played, consented, inspected evidence or operated an independent environment. An unsigned JSON document cannot defeat fabrication of all supporting records. The tooling never fills these fields to unblock itself.

## Native warning policy

`Scripts/gameplay_native_warnings.py` defines `AuraRoleBattleNegativeCasesV1` solely for native regression output. It preserves all warning lines and their hashes. It accepts only exact reviewed message patterns and counts inside the corresponding test's unambiguous start/end interval, with matching per-test export counts:

| Test suffix | Warnings | Deliberate condition |
| --- | ---: | --- |
| Day4.AuthorityRejection | 2 | Missing source/target ASCs are rejected. |
| Day4.DirectCauseDamageBoundary | 1 | Null target is safely rejected. |
| Day4.NeutralDamageCoefficients | 3 | Missing curve rows use neutral coefficients. |
| Day5.AggregateValidationErrors | 8 | Four intentionally nonexistent assets produce aggregated validation errors. |
| Day6.ClientRoleMutationRejected | 1 | Simulated-proxy role write is rejected. |
| Day6.RemovedRoleGrantNotPromoted | 1 | Obsolete role grant is quarantined. |

All originate in `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`. A new message, wrong count, missing/ambiguous test lifecycle, export disagreement, warning outside the reviewed test, startup warning, missing GUID, null-world warning or socket warning remains BLOCKED. Errors still FAIL before classification. This is not a package warning exception list, and does not turn a native PASS into runtime entry.

## Current unimplemented checks

The six remaining capabilities are `buildReceiptProvenance`, `cookedContainerContents`, `publicInputCheckpointSemantics`, `soakMetricsAndIsolation`, `shippingOperationsAndIdentity`, and `engineAnchorSurvey`. The scope remains Draft and actual baseline references remain null. This record documents the implemented remediation subset; it does not call the remaining code or evidence work complete.
