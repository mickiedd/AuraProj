# Num3 Electrocute shock-loop release timing

## Intent

Fix the remaining Num3 Electrocute mismatch shown in the gameplay screenshot: the caster's release animation returned to locomotion before the mouse-following Niagara ray had fully disappeared.

## Root cause

The active `InputTag.3` route is `Content/AbilityDefinitions/Electrocute.xml` through `UAuraDataAbility`. Its `AM_Cast_Electrocute` montage is only 0.1667 seconds, while the `TimedLoop` keeps `NS_ElectricBeam` alive for about 2 seconds. The old Blueprint ability explicitly called `SetInShockLoop`; the active modular route did not. The earlier `DestroyInstance()` to `Deactivate()` change preserved the Niagara tail, but it could not extend the character animation state by itself.

## Changed behavior

- `UAuraDataAbility` now tracks every beam Niagara component spawned by the modular beam node.
- The source actor enters `SetInShockLoop(true)` when the first beam is registered.
- Normal cleanup still calls `UNiagaraComponent::Deactivate()`, allowing `NS_ElectricBeam`'s authored inactive/completion tail to run.
- The source actor leaves the shock loop only after all tracked components broadcast `OnSystemFinished`.
- Delegate and source-state cleanup is reset safely before a later activation; the graph node name and XML contract remain unchanged.

## Validation

- AuraEditor Win64 Development build passed.
- Supervised `AuraAbilityGraph` smoke passed: 26 passed, 0 failed.
- `git diff --check` passed.
- The existing runtime log and screenshot were used to identify the montage/beam timing mismatch. The forward-light-grid overflow warning from the asset's dynamic light renderer remains a separate renderer/performance issue and was not changed in this fix.

## Illustration

[Before/after animation and beam lifecycle diagram](2026-08-30-electrocute-shock-loop-tail.svg)
