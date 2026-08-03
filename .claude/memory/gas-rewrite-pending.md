# GAS Data-Driven Rewrite — Memory: Pending Phases

> Last verified against codebase: 2026-08-03.
> Core architecture (Phases 1, 2, 5) is **implemented**.
> This file records only what is **not yet started** and issues that are **still open**.

---

## Phases Pending

| Phase | Description | Status | Notes |
|-------|-------------|--------|-------|
| **3** | Port remaining player abilities (ArcaneShards, FireBlast, Electrocute, Passives) | ✅ COMPLETED (2026-07-30) | ArcaneShards, FireBlast, Electrocute ported to XML. Passives left as legacy (wrong archetype — passive GE, not action graph). 3 new node types added: SpawnShards, ElectrocuteBeam, + bSetReturnToOwner on SpawnProjectiles. |
| **4** | Enemy abilities + cleanup (legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk) | ⬜ NOT STARTED | Depends on Phase 3. Enemy BPs: GA_EnemyFireBolt, GA_RangedAttack, GA_MeleeAttack, GA_HitReact. |
| **6** | Projectile data-driven (eliminate `BP_FireBolt` / `BP_FireBall`) | ⬜ OPTIONAL, NOT STARTED | `BP_AuraBullet` already gone. High effort — projectile actors are heavily BP-bound. Correctly optional. |

---

## Known Issues — Still Open

Only issues confirmed unfixed as of 2026-07-30 are listed here.
See `.claude/ability-logic-issues.md` for the full catalog (including fixed items).

| # | Issue | Severity | Location | Notes |
|---|-------|----------|----------|-------|

### Lower-priority unfixed items (from ability-logic-issues.md)

| # | Issue | Severity | Notes |
|---|-------|----------|-------|
| H2 | Inconsistent knockback force direction | Medium | ApplyDamage uses `Direction * Mag`, Hitscan/Projectile use `UpVector * Mag`. Data/tuning decision, not clear bug. |
| M7 | PlayMontage returns Success when no montage | Medium | Should return Failure. One-liner fix. |
| M8 | WaitForMontageEvent no EventTag validation | Medium | Defensive — current XML always sets eventTag. |
| M9 | Two damage paths with different context setup | Medium | ApplyDamage vs CauseDamage code smell. |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Medium | Double AdvanceGraph call risk. |
| M11 | WaitForTargetData no active-graph check | Medium | PendingStatus set unconditionally. |
| L13 | Hardcoded Python path in launcher | Low | Fallback to PATH works; cosmetic. |
| L15 | FDamageEffectParams::WorldContextObject never populated | Low | Declared, always nullptr, never read. |
| L16 | ImportFactory CanReimport always false | Low | Must delete + re-import XML assets. |
| L17 | MulticastGunFX no fallback for non-Aura characters | Low | FX silently dropped. |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | Low | Same as H2, different angle. |
| L19 | ApplyDamage hardcodes bIsRadialDamage=false | Low | No XML attribute for radial damage. |
| L20 | CauseDamage vs ApplyDamage ASC access pattern | Low | Should be same ASC, different access. |
| L21 | Projectiles hardcode TargetASC=nullptr | Low | Projectile must resolve ASC on hit. |
| L22 | WaitForTargetData no OnStart validation | Low | Silent hang risk if task creation fails. |

---

## Known Issues — FIXED (verified 2026-07-30)

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

---

## Completed Milestones

- Phase 1 — Framework ✅ (registry, nodes, driver, import factory)
- Phase 2 — MVP port: `FireGun` + `FireBolt` ✅ (XML defs in `Content/AbilityDefinitions/`)
- Phase 5 — GE UAsset elimination ✅ (6 C++ GE classes + `GameplayEffects.json`)
