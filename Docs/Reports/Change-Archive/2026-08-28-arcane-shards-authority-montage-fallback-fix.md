# ArcaneShards Num.2 authority montage fallback fix

## Intent

Restore reliable Num.2 ArcaneShards effects when the activation succeeds but the authoritative server does not observe the locally-authored montage gameplay event.

## Changed behavior

- ArcaneShards now opts into the existing `WaitForMontageEvent` authority fallback with a `0.35` second server delay.
- The server can continue to `SpawnShards` when its local montage notify is unavailable, while the normal client event path remains unchanged.
- The existing authoritative ASC gameplay-cue dispatch then reaches clients so shard effects can display.
- Added a Num.2 XML regression test and strengthened the ArcaneShards graph smoke check to require both a finite timeout and authority fallback.

## Root cause observed in the client/server logs

The client resolved `InputTag.2` to `Abilities.Arcane.ArcaneShards`, passed its input-side checks, and waited for the authoritative result. The server applied the 20 mana cost and eight-second cooldown, but the first activation stayed in `WaitForMontageEvent` and timed out after five seconds without reaching `SpawnShards`. A later activation succeeded, confirming that the input mapping and cue assets were not the primary failure.

## Validation

- AuraEditor Win64 Development build passed.
- `Aura.AbilityGraph.ArcaneShardsAuthorityFallback` passed 1/1.
- `Aura.AbilityGraph` passed 4/4.
- AuraAbilityGraph smoke test passed 26/26, including the ArcaneShards file graph.
- `Aura.UI.WebSkillPanel` passed 2/2.
- FireBolt and Electrocute authority/retrigger contracts remained successful in the same graph suite.
- `git diff --check` passed.

## Illustration

[Open the before-and-after ArcaneShards Num.2 activation flow](2026-08-28-arcane-shards-authority-montage-fallback-fix.svg)
