# Harden Days 44–45 replay idempotence and cancelled-timeline reuse

Date: 2026-09-03. Scope: targeted hardening of the inert Days 44–45 gameplay contract foundation after independent in-app handoff validation. The profile remains fail-closed and is not wired into live input, movement, damage, AI spawning, rewards, persistence, or runtime entry.

[Visual summary](2026-09-03-gameplay-expansion-days-44-45-replay-reset-hardening.svg) · [prior contract archive](2026-09-03-gameplay-expansion-days-44-45-contract-foundation.md)

## Intent

The handoff review identified one real production-contract risk: a bounded request-history entry could be evicted while its lease was still live. A replay could then be mistaken for a new request and consume another lease. The review also required an explicit terminal cancellation reset boundary before timeline reuse.

## Changed behavior

- Heavy and support-field acquisition now prunes expired leases before replay evaluation, checks the bounded request replay cache, and then checks the live lease array by the complete identity key. A matching live payload returns `DuplicateAccepted` with the original serial and rehydrates replay history; a changed payload returns `Conflict`; no second live lease is allocated.
- The replay cache is documented as bounded request history while live lease arrays remain authoritative for active requests. Support-field serial ordering remains coordinator-global and the existing lowest-live-source ownership rule is unchanged.
- `FAuraAttackTimelineState::Reset()` now clears only terminal `Cancelled` state. Active or normally recovering timelines cannot be silently cleared through the reset path.
- Native Day 45 coverage floods 1024 terminal heavy and support requests while retaining the original active lease, then verifies serial preservation, unchanged live counts, and changed-payload conflicts. Native Day 44 coverage verifies cancelled attack rejection before reset, replacement admission after reset, stale-key isolation, and the active-reset guard.

## Validation and limitations

- `python3 Scripts/test_gameplay_day44_45_definitions.py`: 11/11 passed.
- `python3 Scripts/test_gameplay_day42_43_definitions.py`: 4/4 passed.
- `python3 -m py_compile Scripts/test_gameplay_day44_45_definitions.py Scripts/test_gameplay_day42_43_definitions.py`: passed.
- JSON/XML and archive-link checks remain part of the local validation set; the new archive illustration is valid SVG.
- Two independent in-app ChatGPT handoff reviews classified the inert contract as `CONDITIONAL PASS`. The second review found the described heavy/support replay fix closed the prior defect and found no new source-level defect, while noting that exact UE compilation still cannot be independently verified there.
- UE5.5 UHT/UBT/native automation could not run because no UE5.5 engine installation is available in this filesystem context. `git diff --check` remains environment-blocked before the check by the missing `git-lfs` executable.

## Gate classification

The active-lease replay defect is closed at source-contract level, and the cancelled-timeline reset/reuse ambiguity is closed by an explicit terminal-only reset rule and regression coverage. The remaining `DAY40_PACKAGED_EVIDENCE_MISSING`, `ANCHOR_SURVEY_UNVERIFIED`, UE5.5 native-build, replication/runtime, rendered-telegraph, and playable-runtime gates are evidence or environment gates, not permission to activate this inert profile.
