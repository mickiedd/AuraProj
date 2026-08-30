# Playable Candidate Days 21–40 implementation

## Intent

Advance the Playable Candidate roadmap from the frozen Day 21 contract through the Day 40 signoff machinery, with a validation checkpoint for every day.

## Changed behavior

- Added the executable Playable Candidate scope, content manifest, tutorial/recovery/reward definitions, and malformed-fixture checks.
- Added authoritative firearm state and reload cancellation, server-side one-round consumption, semi-auto press-edge activation, input-lease expiry, and owner-only WebUI replay.
- Added server-owned tutorial/recovery state, idempotent Civilian lethal rewards, deterministic UTC max-observed merchant restock, persistence fields, and reconnect generation binding.
- Added redacted run-scoped diagnostics, operations documentation, per-day runners, native automation coverage, and Fast/Candidate/External pipeline artifacts with immutable finalization rules.

## Validation

- AuraEditor Win64 Development build: PASS.
- Native Unreal checks: 20/20 Day 21–40 checks PASS, each with a dedicated log.
- Local day runners: 20/20 PASS.
- Python contract suite: 20/20 PASS.
- Scope/content/visual validators, malformed JSON/XML fixtures, diagnostics redaction, `git diff --check`, Fast stage, and expected negative-path exits: PASS.
- Candidate and External stages correctly report local PASS with packaged/provider status `BLOCKED` and exit code 2; no packaged/public release signoff is claimed.

## Illustration

[Playable Candidate Days 21–40 before/after and validation flow](./2026-08-30-playable-candidate-days-21-40.svg)
