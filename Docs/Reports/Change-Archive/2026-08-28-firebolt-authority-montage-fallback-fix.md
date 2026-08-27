# FireBolt LMB authority montage fallback fix

## Intent

Restore reliable FireBolt visual effects when the LMB activation succeeds but the authoritative server does not observe the locally-authored montage gameplay event.

## Changed behavior

- `WaitForMontageEvent` now supports an opt-in authority fallback delay while retaining the authored montage-event path for normal activations.
- FireBolt configures a `0.35` second server fallback after the cast presentation starts. The authority can therefore continue to `SpawnProjectiles` even when its local montage notify is unavailable.
- The fallback ends the event-wait task safely, advances the authoritative graph, and allows the replicated FireBolt projectiles to create their flight and impact effects.
- Timeout and fallback timers are cleared on event success, task exit, cancellation, and ability teardown so late callbacks cannot advance a completed graph.
- Added a FireBolt XML regression test and strengthened the graph smoke contract to require both a finite timeout and the authority fallback.

## Root cause observed in the client/server logs

The client accepted `Abilities.Fire.FireBolt`, applied cost/cooldown, and completed its local graph boundary. The server also accepted the activation and target data, but had no `SpawnProjectiles` or projectile lifecycle log before `WaitForMontageEvent` timed out and cancelled the ability. Since projectile spawning is authority-only, no replicated projectile existed to display the configured FireBolt Niagara and sound effects.

## Validation

- AuraEditor Win64 Development build passed.
- `Aura.AbilityGraph.FireBoltAuthorityFallback` passed 1/1.
- `Aura.AbilityGraph` passed 3/3.
- AuraAbilityGraph smoke test passed 26/26, including the real FireBolt file graph.
- `Aura.UI.WebSkillPanel` passed 2/2.
- `git diff --check` passed.

## Illustration

[Open the before-and-after FireBolt LMB activation flow](2026-08-28-firebolt-authority-montage-fallback-fix.svg)
