# Bungeeman FireGun projectile presentation

Date: 2026-08-30

Review and final regression pass: 2026-08-31.

## Intent

Restore the missing LMB FireGun bullet presentation for the Bungeeman role: a visible flying tracer and a wall impact mark, without changing the existing authority-owned damage flow.

## Root cause

The runtime logs showed `FireGun` spawning `AuraProjectile` instances that correctly blocked `Landscape_4`, called `OnHit`, and were destroyed. The active `fireGunBullet` definition had no tracer, flight particle, impact particle, or surface-mark asset, and blocking hits discarded `FHitResult::ImpactPoint` and `ImpactNormal`.

## Changed behavior

- `fireGunBullet` now selects native `AAuraBullet` and references the MilitaryWeapDark tracer/flight/impact assets plus a new `M_BulletHoleClean` deferred decal material.
- Projectile definitions now parse and validate tracer mesh, Cascade flight/impact particles, impact sound, surface-mark material, size, and lifetime.
- Blocking hits preserve the impact location and normal through the reliable multicast path. `AAuraBullet` attaches the flight particle, plays the impact particle/cue, and spawns a short-lived oriented bullet-hole decal.
- Overlap impacts and existing FireBall behavior retain their prior one-argument impact path; authority still applies damage and destroys the projectile.
- The multicast carries an explicit surface-hit flag: wall blocks create marks, ordinary overlaps do not. Bullet client collision prediction cannot commit `bHit` and suppress the server's eventual wall mark; other projectile classes retain their existing prediction behavior.
- Definition-backed bullets keep the authored 15-second lifetime and tracer scale. The old three-second cleanup applies only to unconfigured legacy bullets. Flight initialization is idempotent and also runs when configuration arrives after BeginPlay.
- Valid world-origin impact points are preserved. Dedicated servers skip bullet particle, sound and decal playback.

## Validation

- AuraEditor Win64 Development and DebugGame builds passed, including the final deferred-spawn regression check.
- `python Scripts/test_role_battle_days_20.py` passed (15 checks), including the new FireGun presentation contract.
- `python Scripts/test_role_battle_review_fixes.py` passed.
- `ProjectileDefinitions.json` parsed successfully and `git diff --check` passed.
- Unreal automation `Aura.RoleBattle.Day1Catalog` passed with the configured class and asset paths loadable.
- Unreal automation `Aura.RoleBattle.Day20` passed.
- The generated decal package was saved as `/Game/Assets/Effects/Combat/M_BulletHoleClean` and validated as a deferred/translucent material with a 13-node radial opacity graph.
- `Aura.Projectiles.Definitions` and the new `Aura.Projectiles.FireGunPresentation` runtime fixture passed; see `Saved/Logs/FireGunPresentationReviewFinal.log` (exit 0). The fixture uses a separately initialized game world and real decal components. It checks authored lifetime, tracer mesh/scale, origin hit position and projection direction, mark ownership/persistence after projectile destruction, overlap exclusion, authoritative delivery after predicted client hits, and duplicate delivery.
- First fixture run failed because its world had not initialized actor script/RPC dispatch; adding a world context and `InitializeActorsForPlay` corrected the test setup. This failure and the successful rerun are retained in separate logs.
- Existing cook rules explicitly include `/Game/Assets` and `/Game/MilitaryWeapDark`, covering the new JSON-referenced assets.
- Final deferred-spawn test run also verifies the actual initialized velocity is 550 cm/s, not the legacy constructor default. Both `Aura.Projectiles.*` tests passed with exit 0 in `Saved/Logs/FireGunDeferredSpawnFinal.log`.
- Full `Aura.RoleBattle.*` regression run passed **233/233** with exit 0 in `Saved/Logs/FireGunRoleBattleFinal.log`.
- The SVG archive was rendered locally and visually checked for readable layout.

## Independent review

An approved in-app ChatGPT review examined an implementation/evidence packet, not the local repository. Its findings confirmed the lifetime override, overlap/surface conflation, local-versus-authoritative impact race, late configuration, and origin-coordinate issues; those were corrected and locally tested. Its hardcoded decal-lifetime finding came from an abbreviated packet: the actual code already uses the configured `BulletHoleLifeSpan`. Cosmetic loading on stripped dedicated-server builds remains an architectural validation concern; this patch preserves the existing shared config validator and cook policy.

The completed [follow-up browser review](https://chatgpt.com/c/6a945819-2f04-83ec-abc1-fcc736d54cdf) found no proven correctness blocker in the ordinary replicated-bullet path based on the supplied deltas. It emphasized same-frame close-wall impacts and live rendering as remaining verification work. Local source inspection adds an important qualification: UE 5.5's `UNetDriver::InternalProcessRemoteFunctionPrivate` creates a missing actor channel for a relevant, level-ready connection, and `ProcessRemoteFunctionForChannelPrivate` calls `ReplicateActor` before serializing the RPC. Thus absence of a pre-existing channel alone does not prove loss in the default driver; transport/relevancy testing is still needed.

## Verification boundaries

The fixture is headless and directly invokes client multicast delivery to test ordering; it does not prove network-driver transport or visible rendering. Live two-client play (including shots very close to walls), decal appearance, a fresh cooked package, and stripped-cosmetics dedicated-server deployment were not verified. Bullet marks are temporary local cosmetics for clients witnessing an impact, not saved or late-join-replayed world state.

## Illustration

[View the change flow diagram](2026-08-30-bungeeman-firegun-projectile-presentation.svg)
