# 2026-08-11 — Aura damage and modular beam fixes

## Illustration

[Open the before/after diagram](2026-08-11-aura-damage-beam-fixes.svg)

## What changed

- Invalid or incomplete ability-system actor information is rejected safely before damage construction or application.
- Beam target selection checks combat eligibility so friendly or protected actors do not consume chain slots.
- Beam pruning removes actors that transition to a dead state, and optional replacement reacquires an eligible target.

## Validation

- Added regression coverage for invalid damage actor info, beam eligibility, dead-target pruning, and target replacement.
- Smoke validation: 25 tests passed, 0 failed.
