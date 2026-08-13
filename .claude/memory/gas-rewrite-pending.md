# GAS Data-Driven Rewrite — Memory: Pending Phases

> Last synchronized: 2026-08-14.
> Canonical status tracker: `../../Docs/Tracking/GAS-Migration-TODOs.md`.
> This memory file is a compact pointer for agents; it records remaining cleanup/future work and completed milestones, not a separate defect ledger.

---

## Phases Pending

| Phase | Description | Status | Notes |
|-------|-------------|--------|-------|
| **3** | Port remaining player abilities (ArcaneShards, FireBlast, Electrocute, Passives) | ✅ COMPLETED (2026-07-30) | ArcaneShards, FireBlast, Electrocute ported to XML. Passives left as legacy (wrong archetype — passive GE, not action graph). 3 new node types added: SpawnShards, ElectrocuteBeam, + bSetReturnToOwner on SpawnProjectiles. |
| **4** | Enemy abilities + cleanup (legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk) | 🟨 IN PROGRESS | Four enemy XML definitions, native nodes, config grants, and focused validation exist. Legacy packages and source cleanup remain. |
| **6** | Projectile data-driven (eliminate `BP_FireBolt` / `BP_FireBall`) | ⬜ OPTIONAL, NOT STARTED | `BP_AuraBullet` already gone. High effort — projectile actors are heavily BP-bound. Correctly optional. |

---

## Known Issues — Resolved

The previously listed graph issues are resolved in the current implementation. See `Docs/Tracking/GAS-Migration-TODOs.md` for the canonical regression status. The legacy issue catalog is historical and explicitly marked below.

| # | Issue | Severity | Location | Notes |
|---|-------|----------|----------|-------|

### Former lower-priority items

| # | Issue | Severity | Notes |
|---|-------|----------|-------|
| H2 | Inconsistent knockback force direction | Medium | Resolved; directional damage parameters are covered by `SmokeTest_DamageEffectParams`. |
| M7 | PlayMontage returns Success when no montage | Medium | Resolved; configured montage load failures return Failure. |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Medium | Resolved; callback uses the canonical guarded path. |
| M11 | WaitForTargetData no active-graph check | Medium | Resolved; inactive callbacks are ignored and invalid data fails. |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | Low | Resolved; knockback follows the directional death impulse. |

---

## Known Issues — FIXED (verified 2026-08-03)

These were listed as open in the original pending file but are now confirmed fixed:

| # | Issue | How Fixed |
|---|-------|-----------|
| 1 | PlayMontage delegates not wired | `OnInterrupted` + `OnBlendOut` bound in `PlayMontageNode.cpp:59-60` |
| 5 | GC risk (Montage/DamageEffectClass no UPROPERTY) | By design — `UCLASS(Transient)`, GC-scanned via owning `FRoleDefaultInfo`. Not a bug. |
| 6 | SourceObject doesn't replicate | `FindAbilityDefinitionByTag` registry fallback in `DataAbility.cpp:42` |
| 7 | Binary/cache files in git | Launcher artifacts `git rm --cached`, WebView2 cache gitignored (commit `a47c56e`) |
| 8 | Server security (0.0.0.0 bind, path traversal) | Server binds `127.0.0.1` (`ability_graph_server.py:360`), path traversal confined to Content |
| H3 | CheckCost client bypass not implemented | `AuraGameplayAbility.cpp` returns `true` for non-authoritative clients |
| C1 | WebView2 cache + launcher binaries in git | Cleaned in commits `2918713`/`144d96b`/`a47c56e` |
| C2 | Path traversal in Python server | `_confine_to_content` + loopback bind |
| C3 | SourceObject replication null on clients | Process-lifetime definition registry keyed by AbilityTag |
| L14 | CooldownDuration doesn't scale from XML | `DataAbility.cpp:240` now calls `GetValueAtLevel()` |
| M9 | Two damage paths with different context setup | `CauseDamageNode` now matches `ApplyDamageNode`'s `FDamageEffectParams` and shared `ApplyDamageEffect` path |
| L20 | CauseDamage vs ApplyDamage ASC access pattern | `CauseDamageNode` now uses `Ctx.ASC`, matching `ApplyDamageNode` |
| L15 | FDamageEffectParams::WorldContextObject never populated | Data-driven damage producers now set it to the ability avatar |
| L19 | ApplyDamage hardcodes bIsRadialDamage=false | Removed redundant radial-default assignments; the params struct owns the default and radial nodes set explicit values |
| L21 | Projectiles hardcode TargetASC=nullptr | Projectile nodes now seed the target ASC from `Ctx.CursorHit`; impact handling updates it to the collided actor |
| M8 | WaitForMontageEvent authored-tag validation | Validates the requested tag against reflected metadata on the loaded montage notifies |
| L13 | Hardcoded Python path in launcher | Resolves Python executables through Windows `PATH` with `SearchPathW` |
| L16 | ImportFactory CanReimport always false | Stores `SourceFilePath` and reloads the existing definition during reimport |
| L17 | MulticastGunFX no fallback for non-Aura characters | Generic local emitter/sound fallback for non-Aura avatars |
| L22 | WaitForTargetData range validation | Validates hit/actor data and configurable target distance before advancing |

---

## Completed Milestones

- Phase 1 — Framework ✅ (registry, nodes, driver, import factory)
- Phase 2 — MVP port: `FireGun` + `FireBolt` ✅ (XML defs in `Content/AbilityDefinitions/`)
- Phase 3 — Remaining active player abilities ✅ (`ArcaneShards`, `FireBlast`, `Electrocute`; passive migration remains future work)
- Phase 5 — GE UAsset elimination ✅ (6 C++ GE classes + `GameplayEffects.json`)
