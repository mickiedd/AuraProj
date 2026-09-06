# Aura Num.1 FireBlast cast montage

## Intent

Make Aura's `InputTag.1` FireBlast release its radial fireballs through the same montage/event presentation boundary used by the other montage-driven skills.

## Changed behavior

- `FireBlast.xml` now executes `PlayMontage -> WaitForMontageEvent -> SpawnProjectiles`.
- The new `AM_Cast_FireBlast` Aura-skeleton montage authors `Event.Montage.FireBlast` at 0.321s.
- The wait has a five-second recovery timeout and a 0.35-second authority fallback, so a missing client-local notify cannot leave the authoritative ability running forever.
- The existing authored `Event.Montage.FireGun` tag was also registered because the shared montage-event smoke suite exposed that pre-existing registration gap.
- The graph smoke test and native definition test now require the FireBlast montage path, event tag, authored notify, wait configuration, and projectile ordering.

## Validation

- AuraEditor Win64 Development build: PASS.
- `AuraCreateFireBlastMontage`: PASS; generated 0.583s montage from `AM_Cast_FireBolt` with release notify at 0.321s.
- `-AuraAbilityGraphSmokeTest`: PASS, 26 passed / 0 failed.
- `Aura.AbilityGraph.FireBlastAuthorityFallback`: PASS.
- `git diff --check`: PASS.
- The broader `Aura.Abilities.PlayerSkillAuthoritySafety` test remains a known baseline failure because the current tree still lists removed `FireGun.xml` and contains `Captain*.xml` definitions without the required `name` attribute; it is not caused by the FireBlast path.

## Illustration

[FireBlast cast montage flow](2026-09-06-aura-fireblast-cast-montage.svg)
