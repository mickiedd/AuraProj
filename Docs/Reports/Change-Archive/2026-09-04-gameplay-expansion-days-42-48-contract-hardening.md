# Gameplay Expansion Days 42–48 — contract hardening and Day 48 objective foundation

Date: 2026-09-04. Scope: the dependency-safe continuation of the Gameplay Expansion Days 41–60 plan. This record covers review remediation through the inert Day 48 contract boundary; it does not claim the full Days 49–60 runtime plan is finished.

[Visual summary](2026-09-04-gameplay-expansion-days-42-48-contract-hardening.svg) · [Day 48 plan](../../Plans/Gameplay-Expansion-Implementation/Day-48-Objective-Templates.md) · [native report](../../../Saved/Reports/Day42-48-Native-final-verified/index.json)

## Handoff and intent

The requested in-app ChatGPT handoff was attempted through the handoff skill. The only discovered ChatGPT app is the Codex bundle (`com.openai.codex`); Computer Use rejected access to it under the host safety boundary, including the explicit `/Applications/ChatGPT.app` path. No consultant response was available, so the work continued with a local fail-closed review and the latest recorded findings as the decision input.

The intent was to close the seven authority-contract findings recorded on 2026-09-03 and advance the next dependency-safe slice without creating production gameplay authority that the existing Days 41–60 gates cannot yet support.

## Changed behavior

- Mission cells now carry an active generation and reject stale generation death receipts. Encounter death receipts require source-life generation, reject duplicate terminal delivery, and mark the slot terminal before counting it.
- Heavy attack leases include `AttackSequence` in their identity. Attack callbacks now cancel after a bounded recovery grace deadline; accepted evades record an authoritative movement window and reject late sweeps without mutating state.
- Mission snapshots require a strictly newer revision within a run/epoch and reject delayed snapshots from an old epoch. The definition registry requires one exact `DefinitionRevision` and publishes a deterministic `DefinitionsHash` over the ordered source set.
- Day 47A validates duplicate and stale boundary identity before the open-choice guard, so replay remains idempotent and malformed future/out-of-order boundaries fail closed.
- Day 48 adds an authority-only `FAuraObjectiveRunState` reducer and a `CONTRACT_ONLY` objective fixture for Clear, Escort, and InteractHold. The reducer validates an acyclic reachable objective graph, foreign run/epoch rejection, designated escort identities, a single 3-second interact owner with alive/LOS/200-unit checks, cancellation without progress, and extraction only after all required objectives complete.
- The Day 48 reducer is intentionally not wired into production mission components, HUD, AI, RPC, GAS effects, or packaged gameplay. The Day 48 plan now records this precise inert-foundation status.
- Two anonymous Crunch tag helpers were renamed so the full Mac unity build has unique translation-unit symbols; this was a build-validation cleanup, not a gameplay behavior change.

## Validation

- `./BuildEditor.command`: UE 5.5 UHT, compile, link, and Mac editor deployment passed.
- Native `Aura.Gameplay`: 42/42 tests passed, 0 failed test entries, including the six Day 48 cases. Evidence: [`index.json`](../../../Saved/Reports/Day42-48-Native-final-verified/index.json).
- Offline suites passed: Day 42–43 4/4, Day 44–45 11/11, Day 46 4/4, Day 47A 4/4, Day 48 3/3; Python syntax compilation and eight Gameplay JSON parses passed.
- Focused diff and archive checks are being recorded with Git LFS filters disabled because `git-lfs` is not installed on this host. The native editor log still reports existing LFS pointer files as unloadable Crunch assets; those payloads were not rewritten or silently substituted.

## Remaining gates

This is a source-level contract milestone, not a complete Days 41–60 playable implementation. Production objective components, live Rescue AI, HUD/RPC consumers, packaged gameplay evidence, public-input checkpoint semantics, anchor survey, and the later Days 49–60 slices remain deferred or fail-closed. `BLOCKED_RUNTIME_ENTRY`, `DAY40_PACKAGED_EVIDENCE_MISSING`, and `ANCHOR_SURVEY_UNVERIFIED` remain intentionally unchanged.

The required illustration is [archived here](2026-09-04-gameplay-expansion-days-42-48-contract-hardening.svg).
