# FireBolt static test contract update

## Intent

Update `Scripts/test_firebolt_graph.py` after its legacy `BP_FireBolt` assertion failed against the current data-driven projectile format.

## Changed behavior

- Requires `FireBolt.xml` to use `ProjectileDefinition="fireBolt"`.
- Rejects the removed `ProjectileClass` property.
- Parses `Content/Config/ProjectileDefinitions.json` and verifies `fireBolt` maps to native `/Script/Aura.AuraProjectile`.

## Validation

- `python Scripts/test_firebolt_graph.py` — passed.
- Existing Unreal `FireBoltFileGraph` smoke test — passed previously with the same data-driven XML.

## Illustration

[View the before/after flow](2026-08-13-firebolt-static-test-contract.svg).
