# Day 27 — Minimum Fire-Mode Semantics

Status: Planned  
Depends on: Day 25 ammo and Day 26 HUD

## Goal

Implement only the fire cadence required by current content and make it consistent under held input, packet loss, and reload.

## Work

- Freeze the current FireGun mode as **semi-auto** for this candidate: one accepted shot per press, no held-input auto-repeat, and any automatic mode is deferred to a later scope revision.
- Encode cadence and server rate limits in the weapon/ability data contract.
- Route press/hold/release through existing ability input semantics while the server remains the final acceptance authority.
- Make rate-limit rejection visible as a normal HUD state, not a silent failure.

## Detailed execution contract

### Files to inspect or modify

- **Definition:** `Content/AbilityDefinitions/FireGun.xml`, `Content/Config/RoleConfig.json`, and the XML schema/content validator.
- **Input/authority:** active AuraAbilityGraph input nodes, `AuraFireGun`/FireGun execution, `AuraPlayerController`, server time/cooldown state, and Day 25 ammo state.
- **Presentation/tests:** Day 26 HUD rejection event, `Source/Aura/Private/Tests/AuraRoleBattleDay27Tests.cpp`, `RunPlayableCandidateDay27FireMode.ps1`, and `day-27-fire-mode.json`.

### Fire-mode contract

The candidate definition is `SemiAuto`: one accepted shot per press, release required before another shot, no held-input auto-repeat. The server reads the configured minimum shot interval and never trusts a client timestamp or cadence value. Reload, dead state, invalid target, cooldown, and empty magazine are terminal rejection reasons for that request.

### Detailed steps

1. Add or validate explicit `fireMode=SemiAuto` and `minimumShotInterval` fields on the active FireGun definition; reject missing/invalid values.
2. Ensure press/hold/release input has one request ID stream and that only the owning controller can submit it.
3. Implement server cadence using one authoritative time source and a per-player/weapon last-accepted-shot record; avoid client-side rate decisions.
4. Order checks as session/ownership, life/loadout, target/range/rule, reload/cooldown, ammo, then commit shot and cadence state.
5. Return stable rejection codes to the Day 26 HUD; never treat an ignored held input as a successful shot.
6. Exercise duplicate, reordered, delayed, burst, packet-loss, cooldown-boundary, reload-overlap, and reconnect requests in both topologies.
7. Compare XML mode, server accepted-count, ammo revision, projectile/damage result, and HUD result to prove one definition is authoritative.

### Named automation and commands

- Native tests: `Aura.RoleBattle.Day27.SemiAutoDefinition`, `OnePressOneShot`, `HeldInputNoRepeat`, `CadenceLimit`, `ReorderedRequests`, `ReloadOverlap`, `EmptyAmmo`, and `StableRejectionCode`.
- Run `RunPlayableCandidateDay27FireMode.ps1 -Mode Listen` and `-Mode Dedicated` with 100 rapid/reordered requests per client; record accepted/rejected counts and the configured interval.
- The JSON artifact must prove accepted shots never exceed the configured cadence and that each rejection leaves ammo, damage, and cooldown state unchanged.

## Validation and evidence

- Test cadence, duplicate/reordered requests, packet loss, reload overlap, cooldown overlap, and server-time boundary cases.
- Run listen/dedicated two-client checks with malicious rapid-fire input.
- Prove the XML ability, ammo state, and HUD all resolve the same fire-mode definition.

## Deep-review closure

- **Owner surfaces:** `Content/AbilityDefinitions/FireGun.xml`, the weapon/ability data contract, server input acceptance, and the Day 26 HUD rejection state.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Fire-Mode-Contract.md` and `day-27-fire-mode.json` containing the configured minimum interval, accepted/rejected request counts, server-time window, and test seed.
- **Gate:** 100 rapid/reordered/duplicated requests in each topology cannot produce more accepted shots than the configured cadence permits; held input produces no second shot without a new press; reload and cooldown overlap return bounded rejection results rather than silently mutating ammo.

## Completion gate

Current weapon content fires with predictable cadence and cannot exceed server-defined rate or ammo limits. Durability remains out of scope.
