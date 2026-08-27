# Player-skill authority safety coverage

## Intent

Prevent the FireBolt and ArcaneShards missing-effect failure pattern from returning in any input-bound player skill.

## Changed behavior

- Added a data-driven automation test that scans every input-bound XML definition in `Content/AbilityDefinitions`, currently covering FireBlast, ArcaneShards, Electrocute, FireBolt, and FireGun.
- Direct skills must contain a reachable authority effect node.
- Montage-gated skills must have a valid authored montage event, finite timeout, authority fallback before timeout, immediate `PlayMontage → WaitForMontageEvent` ordering, and a downstream authority effect node.
- Added the authority fallback configuration to the previously uncovered FireGun and Electrocute montage waits.
- Strengthened the FireGun, ArcaneShards, and Electrocute graph smoke checks and kept the inline FireGun fixture aligned with production XML.

## Failure pattern now covered

If a skill charges cost, applies cooldown, enters `WaitForMontageEvent`, and has no server-visible montage notify, the inventory-wide test and per-file smoke contracts require the fallback and the downstream effect path. A missing fallback, invalid event, missing montage, reordered wait, or missing authority effect fails validation before the skill can ship.

## Validation

- AuraEditor Win64 Development build passed.
- `Aura.Abilities.PlayerSkillAuthoritySafety` passed 1/1.
- Full `Aura.Abilities` suite passed 10/10.
- AuraAbilityGraph smoke test passed 26/26, including FireBlast, FireGun, FireBolt, ArcaneShards, and Electrocute graph checks.
- XML parsing and `git diff --check` passed.

## Illustration

[Open the all-player-skill authority safety coverage map](2026-08-28-player-skill-authority-safety-coverage.svg)
