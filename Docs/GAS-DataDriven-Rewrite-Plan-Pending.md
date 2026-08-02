# Data-Driven GAS Rewrite — Pending Phases & Known Issues

> Last verified: 2026-07-30. See `.claude/memory/gas-rewrite-pending.md` for the authoritative status.

## Phases NOT Started (from Plan §10)

### Phase 3 — Port remaining player abilities ✅ COMPLETED (2026-07-30)
- ArcaneShards, FireBlast, Electrocute ported to XML definitions in `Content/AbilityDefinitions/`
- Passives (HaloOfProtection, LifeSiphon, ManaSiphon) left as legacy — wrong archetype (passive GE, not action graph)
- 3 new XML node types added: `SpawnShards`, `ElectrocuteBeam`, + `bSetReturnToOwner` property on `SpawnProjectiles`
- RoleConfig.json updated: Aura role now has FireBlast, ArcaneShards, Electrocute in `startupAbilityDefinitions`
- Build passes, 8/8 smoke tests pass

### Phase 4 — Enemy abilities + cleanup 🟨 IN PROGRESS (2026-08-03)
- Four enemy XML definitions, class-based JSON grants, native enemy graph nodes, and focused validation tests are implemented and pass compile-only validation.
- Legacy BPs (GA_EnemyFireBolt, GA_RangedAttack, GA_MeleeAttack, GA_HitReact) remain pending Asset Registry, reload, automation, and gameplay gates.
- Legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk (dead code for DataAbility path)
- Depends on Phase 3 completing first
- Remove unused GE Blueprint assets after migration complete

### Phase 6 (optional, future) — Projectile data-driven ⬜ NOT STARTED
- Move projectile mesh/FX/impact params into a data asset
- Eliminate `BP_FireBolt` / `BP_FireBall` Blueprint assets (`BP_AuraBullet` already gone)
- High effort — projectile actors are heavily BP-bound (mesh, FX, collision, overlap handlers)
- Correctly marked optional

---

## Known Issues — Still Open (verified 2026-07-30)

| # | Issue | Severity | Location |
|---|-------|----------|----------|
| 2 | Dead declarations — `CreateNodeByClassName`, `BuildAndExecuteGraph` | Medium | `AbilityDefinition.h`, `DataAbility.h` |
| 3 | Unused XML properties — `TargetFromContext`, `ScatterRadius` | Medium | Multiple node `.cpp` files |
| 4 | Fake smoke tests — log-only, no assertions | Medium | `AuraAbilityGraphModule.cpp` |
| H2 | Inconsistent knockback force direction (Direction vs UpVector) | Medium | `ApplyDamageNode`, `HitscanTraceNode`, `SpawnProjectileNode` |
| M7 | PlayMontage returns Success when no montage | Medium | `PlayMontageNode.cpp:29` |
| M8 | WaitForMontageEvent no EventTag validation | Medium | `WaitForMontageEventNode.cpp` |
| M9 | Two damage paths with different context setup | Medium | `ApplyDamageNode` vs `CauseDamageNode` |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Medium | `WaitForMontageEventNode.cpp` |
| M11 | WaitForTargetData no active-graph check | Medium | `WaitForTargetDataNode.cpp` |
| L13 | Hardcoded Python path in launcher | Low | `AuraAbilityGraphLauncher.cpp:193` |
| L14 | CooldownDuration doesn't scale from XML | Low | `AbilityDefinition.cpp:138` |
| L15 | WorldContextObject never populated | Low | `AuraAbilityTypes.h:16` |
| L16 | ImportFactory CanReimport always false | Low | `AbilityDefinitionImportFactory.cpp` |
| L17 | MulticastGunFX no fallback for non-Aura characters | Low | `MulticastGunFXNode.cpp` |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | Low | `HitscanTraceNode.cpp` |
| L19 | ApplyDamage hardcodes bIsRadialDamage=false | Low | `ApplyDamageNode.cpp:69` |
| L20 | CauseDamage vs ApplyDamage ASC access pattern | Low | `CauseDamageNode.cpp:44` |
| L21 | Projectiles hardcode TargetASC=nullptr | Low | `SpawnProjectileNode.cpp`, `SpawnProjectilesNode.cpp` |
| L22 | WaitForTargetData no OnStart validation | Low | `WaitForTargetDataNode.cpp` |

---

## Known Issues — FIXED (verified 2026-07-30)

| # | Issue | How Fixed |
|---|-------|-----------|
| 1 | PlayMontage delegates not wired | `OnInterrupted` + `OnBlendOut` bound in `PlayMontageNode.cpp` |
| 5 | GC risk (Montage/DamageEffectClass) | By design — `UCLASS(Transient)`, GC-scanned via owner. Not a bug. |
| 6 | SourceObject doesn't replicate | `FindAbilityDefinitionByTag` registry fallback in `DataAbility.cpp` |
| 7 | Binary/cache files in git | Launcher artifacts removed, WebView2 cache gitignored |
| 8 | Server security | Server binds `127.0.0.1`, path traversal confined to Content |
| H3 | CheckCost client bypass | `AuraGameplayAbility.cpp` returns `true` for non-authoritative clients |
| C1 | WebView2 cache + launcher binaries in git | Cleaned in commits `2918713`/`144d96b`/`a47c56e` |
| C2 | Path traversal in Python server | `_confine_to_content` + loopback bind |
| C3 | SourceObject replication null on clients | Process-lifetime definition registry keyed by AbilityTag |

---

## Verification Notes
- Core architecture: ✅ IMPLEMENTED (Phases 1, 2, 5 complete)
- Build passes on UE 5.5.1
- Smoke test: `AuraAbilityGraphSmokeTest` (8 tests, all passing)
- Two abilities fully ported: `FireBolt.xml`, `FireGun.xml` in `Content/AbilityDefinitions/`
- RoleConfig.json references XML definitions for Aura (FireBolt) and BungeeMan (FireGun)
