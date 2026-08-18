# FireBolt self-impact collision fix

## Intent

Stop FireBolt's client-side explosion from appearing on the casting character when the projectile is released toward the ground or another collision target.

## Log diagnosis

The server target-data logs correctly reported `Landscape_1`/`Landscape_5` as the target. The client then logged `Overlap ... Other=BP_AuraCharacter_C_0` immediately at the muzzle, followed by `OnHit ... HasAuth=0`. Because `DamageEffectParams.SourceAbilitySystemComponent` is not replicated, the client could not resolve the source avatar and spawned the impact effect on its own character before the server reached the actual collision.

## Changed behavior

- Data-driven and legacy projectile spawners now set the projectile owner to the avatar and the instigator to the avatar pawn when available.
- `AAuraProjectile` rejects overlaps/hits against its source avatar using the replicated owner/instigator identity, with the existing server-side ASC check retained as a fallback.
- Real pawn targets and WorldStatic/WorldDynamic collisions continue through the normal impact and damage path.
- Added structural regression checks for the source-identity guard and graph spawn contract.

## Validation

- `python Scripts/test_firebolt_graph.py` — passed.
- UE 5.5 Live Coding compile for `AuraEditor Win64 Development` — passed, 8 actions completed.
- `git diff --check` — passed.
- The normal non-Live-Coding build was also attempted but correctly refused while the editor's Live Coding session was active; no editor process was terminated.

## Illustration

[View the before/after impact flow](2026-08-18-firebolt-self-impact-fix.svg)
