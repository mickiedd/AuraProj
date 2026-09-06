# Aura Num.1 FireBlast validation fixes

## Intent

Verify the supplied FireBlast review packet and close the two confirmed P2 gaps before treating Aura's `InputTag.1` skill as fully validated.

## Changed behavior

- `WaitForMontageEvent` now has an explicit one-winner completion latch. Montage notify, authority fallback, timeout, cancellation, and late same-frame callbacks cannot advance the graph more than once; timers and the gameplay-event task are cleared on completion.
- Full-circle projectile spreads now use `Spread / Count`, so FireBlast's `Count=12, Spread=360` produces twelve unique radial directions at approximately 30° spacing. Partial spreads retain endpoint-inclusive spacing.
- The graph smoke harness now covers notify-wins, fallback-wins, both boundary callback orders, duplicate-event inertness, and the twelve-direction full-circle regression.

## Validation

- AuraEditor Win64 Development build: PASS.
- AuraAbilityGraph smoke suite: PASS, 27 passed / 0 failed.
- `Aura.AbilityGraph.FireBlastAuthorityFallback`: PASS.
- `Aura.Abilities.PlayerSkillAuthoritySafety`: reproduces the packet's unrelated baseline failures in three malformed `Captain*.xml` files and the removed `FireGun` expectation; no FireBlast failure.
- Packet SHA-256 manifest: all three supplied packet files matched.
- FireBlast XML contract: `InputTag.1`, `PlayMontage -> WaitForMontageEvent -> SpawnProjectiles`, `Count=12`, `Spread=360`, fallback `0.35`.
- `git diff --check`: PASS.

The packet's P3 provenance/build-log omissions remain documentation-quality gaps, not runtime defects, and were not broadened into this source fix. Independent browser handoff was not sent because the request did not authorize transmitting private repository evidence to Google AI Studio; local build, smoke, focused automation, and diff validation were used instead.

## Illustration

[FireBlast validation fixes](2026-09-06-aura-fireblast-validation-fixes.svg)
