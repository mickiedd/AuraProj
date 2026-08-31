# Day 41 — Gameplay Contract and Baseline

Status: Tooling and baseline-warning remediation implemented; full evidence verification and Draft/baseline runtime entry remain BLOCKED. See the [current remediation record](../../Reports/Gameplay-Baseline-Blocker-Remediation-2026-08-31.md) and [earlier deep review](../../Reports/Gameplay-Expansion-Deep-Review-2026-08-31.md).  
Depends on: Days 21–40 evidence; no assumed packaged PASS.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

Evidence schemas and fail-closed validation: [baseline evidence contract](Baseline-Evidence-Contract.md).

## Player outcome

Turn the proposed expedition into a frozen, measurable scope and prove that the existing candidate is a safe starting point.

## Exact change surfaces

- Existing: `RunPlayableCandidate.ps1`, `Scripts/RunPlayableCandidateDay40Finalize.ps1`, `Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json`, and the exact Day 40 candidate/lane/soak records.
- Existing: `Content/Config/LevelConfig.json`, `RoleConfig.json`, `BattleZones.json`, `Source/Aura/Private/Game/AuraGameModeBase.cpp`, and `Private/Character/AuraEnemy.cpp`.
- New: `gameplay-expansion-scope.json` beside this plan; `RunGameplayExpansion.ps1`, `Scripts/ValidateGameplayExpansionScope.ps1`, `Scripts/gameplay_expansion.py`, `Scripts/gameplay_baseline_readers.py`, `Scripts/gameplay_native_warnings.py`, their three `test_gameplay_*.py` files, and the Windows job-object helper `Scripts/GameplayExpansionProcess.cs`. Day 41 tests evidence tooling in Python and runs the native `Aura.RoleBattle` suite. It adds the bounded asset regression `Aura.RoleBattle.Day41.Assets.SoundCueGraphGuids`, not a new mission/profile runtime.

- Baseline remediation: `Source/AuraEditor/Private/AuraEditorModule.cpp::RegisterMenus` skips commandlet/headless/UI-disabled toolbar probing while retaining normal editor registration and EndPIE cleanup.
- Baseline remediation: resave only `Content/MilitaryWeapDark/Sound/Rifle/Rifle_ImpactSurface_Cue.uasset` and `RifleB_Fire_Cue.uasset`; verify their node GUIDs in `Source/AuraEditor/Private/Tests/AuraBaselineAssetTests.cpp`. Detach the weapon before clearing the body mesh in `Source/Aura/Private/Character/AuraCharacterBase.cpp`; avoid world-required calls for null-context role catalog access in `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp`. `Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.{h,cpp}` and `AuraRoleBattleTests.cpp` verify equipment reattachment and use a valid empty-shell attachment.

## Data and authority contract

The planned profile is server-selected GameplayExpansionV1; server selection is implemented and tested on Day 42. Old candidate scope and evidence are immutable. The scope has explicit Draft and Frozen phases. Draft fixes the proposed feature inventory, lanes, seeds, contracts and numeric targets, but uses null for unavailable candidate, survey, trace or human evidence and lists typed blockers. Draft is a valid tooling input and can never produce READY. Frozen requires validated Day 40 provenance, real baseline observations, measured hardware/budgets and the collision/navigation survey; a scope hash is separate from source-at-freeze. Do not fill missing evidence with plausible paths or invented values. Unsupported engine/package stages return BLOCKED.

## Numbered implementation steps

1. Inspect explicitly supplied Day 40 artifacts and recursively verify source ancestry, hashes, all four old lanes, soak, actual package bytes and the underlying journey/operations records. Reject schema-1 local-contract stubs, self-asserted PASS, zero-cycle soak and missing provenance. If absent, issue prerequisite-remediation.json naming missing records; finish tooling/design work but block Day 42 runtime entry. Do not search for the latest PASS. The inherited Shipping identity and second-developer operations requirements remain prerequisites where the Day 40 contract requires them; a separate external-provider row does not waive them.
2. Record current HEAD, dirty paths and engine version; trace the actual role/ability/AI/reward/save/HUD paths against the analysis document. Reproduce or downgrade suspected duplicate AI/spawn failures rather than declaring them proven from a header.
3. Run the old Fast regression and separately build/run the existing native Aura.RoleBattle suite, parsing actual exported results and rejecting empty discovery. Record dirty-source diagnostics honestly. Existing packaged runners can supply narrow diagnostic evidence only until their public-input journey and operations gaps are remediated.
4. Play the existing loop in both roles when baseline binaries are available. Record objective clarity, damage causes, shots/reload usage, time to first action, boredom points and a five-minute trace; no retrospective invented scores.
5. Survey StartupMap with engine collision/navigation queries; record reachable hub/cell/escort/boss anchors, safe-zone overlap, actual geometry assets and screenshots. Missing usable space creates a bounded layout remediation dependency.
6. Implement the thin runner/validator interface and commit a Draft scope with explicit missing-evidence blockers when necessary. Freeze only after all required inputs validate. Report tooling status separately from runtimeEntry READY/BLOCKED_RUNTIME_ENTRY; changing a fixed design requires a new scope hash and dependent evidence invalidation.

## Named tests and commands

Tooling file: `Scripts/test_gameplay_expansion.py`; Python unittest cases with temporary fixture roots and real file hashes, not source-token assertions. Native baseline: existing `Aura.RoleBattle` tests.

- ScopeRequiredFields — malformed schema/profile/lane/budget fields fail; absent evidence is explicitly null in Draft and rejected in Frozen.
- DraftNeverReady — a structurally valid Draft remains BLOCKED_RUNTIME_ENTRY even if its diagnostic checks pass.
- BaselineEvidenceBinding — altered bytes/hash, duplicate lane, dirty final source, non-ancestor source and missing/zero soak cannot make runtimeEntry READY.
- StubEvidenceRejected — local-contract, unexecuted packaged and incomplete operations records cannot satisfy the predecessor gate.
- MissingAndEscapingArtifacts — missing files, traversal, unsafe RunId and unbound artifact references fail closed.
- EmptyNativeReportRejected — missing/empty/failed native results are nonzero; an old unrelated report cannot be reused.
- LegacyScopeUnchanged — old scope hash stays identical after running the new adapter.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 41 -Stage Preflight -RunId d41-preflight
./RunGameplayExpansion.ps1 -Day 41 -Stage Fast -RunId d41-fast
python Scripts/test_gameplay_expansion.py --day 41 --output Saved/Reports/GameplayExpansion/day41-tooling.json
```

Preflight performs evidence/scope validation without building. Fast additionally runs tooling tests, old local regressions and the actual native baseline. Both return exit 2 while runtime entry is blocked, even if all available local tests pass; the result distinguishes those facts. Packaged is recognized but explicitly BLOCKED until an actual baseline driver exists. No static-only result completes Day 41. ProfileSelectionServerOnly is a Day 42 native integration test.

## Fixtures and topologies

BaselineAudit: old four packaged role/topology rows plus S-A/S-B observation; no new gameplay is required yet. Use the recorded reference machine, 1080p Medium and a five-minute warmup.

## Failure and timeout semantics

Missing packaged or human baseline is a named BLOCKED row. Build/package limits come from the shared contract; never let a native/static PASS satisfy the entry gate. New layout work stays blocked if navmesh or safe-zone placement is unverified. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-41.json, prerequisite-remediation.json if needed, baseline-observations.md, baseline.trace.utrace, hardware.json, arena-anchor-survey.json, scope hash and source/candidate provenance. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

The tooling subset can be complete after its behavioral tests and independent review, with a valid Draft and explicit blockers. Day 41 as a whole and runtime entry are READY only after validated Day 40 PASS, both-role baseline observations, hardware/trace and usable mission-space evidence, then a Frozen scope. Otherwise Day 42 onward remains blocked; neither Draft nor a stage-local PASS is permission to start runtime changes.

## Defer / anti-goals

No enemy roster expansion, implementation of missions, new map purchase, engine upgrade, or unaudited scope shortcuts.
