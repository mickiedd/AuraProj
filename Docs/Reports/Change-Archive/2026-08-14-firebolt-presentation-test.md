# FireBolt presentation regression coverage — 2026-08-14

## Intent

Ensure the FireBolt projectile cannot appear in gameplay with missing or invalid visual/audio presentation assets after the authority-boundary fix.

## Test coverage added

`FAuraProjectileDefinitionsTest` (`Aura.Projectiles.Definitions`) now checks that:

- FireBolt's flight trail and impact effect resolve as `UNiagaraSystem` assets.
- FireBolt's impact and looping sounds resolve as `USoundBase` assets.
- Every presentation path is non-empty and has a valid loaded object path.
- The existing native projectile configuration, collision, speed, and XML contract checks remain active.
- In a running automation world, the projectile's Niagara flight-trail component is also required after `BeginPlay`.

## Validation

- `AuraEditor Win64 Development` build passed after compiling `AuraGameplayDefinitionTests.cpp`.
- `python Scripts/test_firebolt_graph.py` passed.
- SVG XML parsing passed.
- `git diff --check` passed.
- The focused command-line automation runner could not execute the test because the installed editor reports the existing project module preload failure: `Aura` imports `UnrealEditor-AuraAbilityGraph.dll`, but the command-line module search path does not resolve the plugin binary before the primary game module. This is recorded as an environment/launcher limitation, not a test pass.

## Visual summary

[View the FireBolt presentation test diagram](./2026-08-14-firebolt-presentation-test.svg)
