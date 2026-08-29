# Num3 Electrocute beam graceful release

## Intent

Fix the active Num3 Electrocute release so the light ray is not hard-cut at the end of the 2-second channel. The active route is `InputTag.3` → `Content/AbilityDefinitions/Electrocute.xml` → `NS_ElectricBeam`.

## Changed behavior

- `ModularBeamNodes.cpp` now calls `UNiagaraComponent::Deactivate()` during normal beam cleanup and dead-target pruning.
- Niagara is therefore allowed to run `NS_ElectricBeam`'s authored completion/fade tail and auto-destroy after the ray is fully gone.
- The `DestroyBeamVisuals` graph node name remains unchanged for XML/registry compatibility.
- `AM_Cast_Electrocute` remains a short 0.1667-second cast clip by design; it is separate from the beam's release lifetime.

## Asset/code evidence

The Niagara asset contains `System.bCompleteOnInactive`, `bCanDieWhenEmitterDeactivates`, and emitter loop/lifetime settings. The previous `DestroyInstance()` path immediately released the Niagara system, which bypassed that authored tail.

## Validation

- AuraEditor Win64 Development build passed.
- Direct `-AuraAbilityGraphSmokeTest` passed: 26 passed, 0 failed, including Electrocute graph parsing, endpoint tracking, and cleanup safety.
- Focused source/XML/asset contract checks passed for the active Num3 mapping, 2-second channel, graceful deactivation, and Niagara completion flags.
- `git diff --check` passed.

## Illustration

[Before/after release lifecycle diagram](2026-08-29-electrocute-beam-graceful-tail.svg)
