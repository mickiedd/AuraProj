# Num3 Electrocute lifecycle hardening

## Intent

Validate the reported follow-up risks after the initial Num3 Electrocute fix and prevent rapid reactivation or abnormal teardown from cutting the Niagara release tail or leaving the caster in the shock-loop animation.

## Findings

- The rapid-reactivation concern was real: the old activation path reset beam tracking even when a prior `Deactivate()` call had left Niagara rendering its authored inactive tail.
- The teardown concern was real as an abnormal-path risk: `OnSystemFinished` is the normal completion signal, but forced component/actor/ability teardown can bypass it.
- Normal ability object lifetime is already isolated by `InstancedPerActor`, so the fix extends that lifecycle rather than moving the state to a global/shared object.

## Changed behavior

- Activation now prunes completed weak beam references and preserves live tails across a rapid reactivation.
- Beam tracking retains per-component `OnSystemFinished` delegates and uses a weak 0.1-second fallback poll for completion paths that do not broadcast.
- Source `OnEndPlay` and ability `BeginDestroy` clear delegates and timer state; actor teardown does not call back into an ending actor.
- A defensive source handoff clears the previous source loop before enabling the new source, while leaving tracked Niagara tails intact.
- The normal release path remains `Deactivate()` → authored Niagara tail → all components finished → `SetInShockLoop(false)`.

## Validation

- AuraEditor Win64 Development build passed.
- Supervised `AuraAbilityGraph` smoke passed: 26 passed, 0 failed.
- Lifecycle/static assertions passed for `InstancedPerActor`, `Deactivate()`, track-before-activate ordering, completion callbacks, weak polling, and source-end-play cleanup.
- `git diff --check` passed.
- Rendered multiplayer PIE/latency validation was not available in this pass; the smoke runner is null-RHI.
- The existing Niagara dynamic-light grid overflow warning remains a separate renderer-capacity issue and was not changed.

## Illustration

[Lifecycle hardening before/after diagram](2026-08-30-electrocute-lifecycle-hardening.svg)
