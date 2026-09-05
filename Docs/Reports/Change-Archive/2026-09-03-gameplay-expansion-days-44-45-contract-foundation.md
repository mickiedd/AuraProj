# Days 44–45 attack, evasion, and enemy-archetype contract foundation

Date: 2026-09-03. Scope: inert authority-contract work for the Days 44–45 gameplay expansion plans, following the handoff review. The new gameplay profile remains fail-closed and is not wired into live input, movement, damage, GameMode, AI spawning, rewards, or persistence.

[Visual summary](2026-09-03-gameplay-expansion-days-44-45-contract-foundation.svg) · [Day 44 contract](../../Plans/Gameplay-Expansion-Implementation/Day-44-Telegraphs-and-Evasion.md) · [Day 45 contract](../../Plans/Gameplay-Expansion-Implementation/Day-45-Enemy-Archetypes.md)

## Intent and changed behavior

The next safe increment turns the handoff-approved Day 44–45 seams into strict, testable contracts:

- Attack timelines carry Run, epoch, encounter generation, immutable driver, source entity/life generation, and authority sequence identity. Server-derived windup/impact/recovery times enforce the 0.85-second minimum, reject stale callbacks, and cancel pre-impact attacks without damage side effects. A replicated component carries state only; cue presentation remains a separate rendered-frame concern.
- Evade admission is a pure authority policy with explicit rejection results, a 350-unit/0.25-second movement contract, a 2.5-second cooldown, one charge, horizontal normalized direction, authoritative swept-resolution results, and an explicit cancellation plan for owned interaction/reload/channel state. It never grants invulnerability or invokes movement.
- The five-source definition registry now strictly parses the Raider, Lancer, Bulwark, and Disruptor archetypes, opaque attack shape/telegraph references, the 2-heavy-attack and 2-token budgets, Bulwark’s 120°/60% counter contract, and Disruptor’s two-second/20%/600-unit support field.
- Encounter coordination now provides atomic heavy participant leases, bounded idempotent request history, stale driver/source-life rejection, expiry and generation invalidation, and lowest-live-source-lease support ownership. A generic generation-safe status/interrupt ledger provides cleanup reasons and a two-second interrupt immunity seam without applying GAS effects.

## Validation and limitations

- `python3 Scripts/test_gameplay_day44_45_definitions.py`: 10/10 passed.
- `python3 Scripts/test_gameplay_day42_43_definitions.py`: 4/4 passed.
- Python compilation for both suites passed; the six JSON documents used by the current foundation parse through the existing contract suite.
- Native automation coverage was added in `Source/Aura/Private/Tests/AuraGameplayDay44Tests.cpp` and `Source/Aura/Private/Tests/AuraGameplayDay45Tests.cpp`, but it could not run because no UE 5.5 engine installation is available in this filesystem context. `git diff --check` is also environment-blocked by the missing `git-lfs` executable.
- The handoff review classified these two increments as safe to implement but explicitly rejected runtime activation until packaged Day 40/41 evidence, baseline/operations evidence, surveyed anchors, and the engine/runtime gates are available. The existing `DAY40_PACKAGED_EVIDENCE_MISSING` and `ANCHOR_SURVEY_UNVERIFIED` blockers remain authoritative.

This archive records contract completion for the inert Day 44–45 foundation, not completion of the full Days 41–60 playable runtime. Days 46–60 remain planned or evidence-gated.
