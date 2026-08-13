# Data-Driven GAS Rewrite — Pending Phases & Known Issues

> Last synchronized: 2026-08-14. The canonical status is `../../Docs/Tracking/GAS-Migration-TODOs.md`; this file is a compact compatibility summary.

## Pending and Recently Completed Phases

### Phase 3 — Port remaining player abilities ✅ COMPLETED (2026-07-30)
- ArcaneShards, FireBlast, Electrocute ported to XML definitions in `Content/AbilityDefinitions/`
- Passives (HaloOfProtection, LifeSiphon, ManaSiphon) left as legacy — wrong archetype (passive GE, not action graph)
- 3 new XML node types added: `SpawnShards`, `ElectrocuteBeam`, + `bSetReturnToOwner` property on `SpawnProjectiles`
- RoleConfig.json updated: Aura role now has FireBlast, ArcaneShards, Electrocute in `startupAbilityDefinitions`
- Build passes, full headless smoke suite passes (22 checks, 0 failures; verified 2026-08-04)

### Phase 4 — Enemy abilities + cleanup 🟨 IN PROGRESS (2026-08-03)
- Four enemy XML definitions, class-based JSON grants, native enemy graph nodes, and focused validation tests are implemented; the current tracker records the parse/config/build gates as complete.
- Legacy BPs (GA_EnemyFireBolt, GA_RangedAttack, GA_MeleeAttack, GA_HitReact) remain pending Asset Registry, reload, automation, and gameplay gates.
- Legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk (dead code for DataAbility path)
- Phase 3 is complete; remaining work is Asset Registry/reference/reload/automation/gameplay verification, legacy package removal, and source cleanup.
- Remove unused GE Blueprint assets after migration gates complete.

### Phase 6 (optional, future) — Projectile data-driven ⬜ NOT STARTED
- Move projectile mesh/FX/impact params into a data asset
- Eliminate `BP_FireBolt` / `BP_FireBall` Blueprint assets (`BP_AuraBullet` already gone)
- High effort — projectile actors are heavily BP-bound (mesh, FX, collision, overlap handlers)
- Correctly marked optional

---

## Known Issues — Resolved (verified 2026-08-04)

| # | Issue | Severity | Resolution |
|---|-------|----------|----------|
| H2 | Inconsistent knockback force direction | Medium | Directional knockback unified; covered by `SmokeTest_DamageEffectParams` |
| M7 | PlayMontage missing montage behavior | Medium | Configured load failures return Failure; intentional empty montage remains a no-op |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Medium | Callback routes through the canonical guarded path |
| M11 | WaitForTargetData callback lifecycle | Medium | Inactive graph callbacks are ignored; invalid target data fails |
| L14 | CooldownDuration doesn't scale from XML | Low | Cooldown duration is evaluated at the active ability level |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | Low | Hitscan knockback follows its directional death impulse |

---

## Known Issues — FIXED (verified 2026-08-03)

| # | Issue | How Fixed |
|---|-------|-----------|
| 1 | PlayMontage delegates not wired | `OnInterrupted` + `OnBlendOut` bound in `PlayMontageNode.cpp` |
| 2 | Dead declarations | Removed the unused `CreateNodeByClassName` and `BuildAndExecuteGraph` declarations |
| 3 | Unused XML properties | Removed the ignored `TargetFromContext` and `ScatterRadius` properties from runtime/editor schemas and authored definitions |
| 4 | Fake smoke tests | `SmokeTest_NodeRegistry` validates all concrete registrations; `SmokeTest_SequenceExecution` executes a real two-child sequence |
| M9 | Two damage paths with different context setup | `CauseDamageNode` now matches `ApplyDamageNode`'s `FDamageEffectParams` and shared `ApplyDamageEffect` path |
| 5 | GC risk (Montage/DamageEffectClass) | By design — `UCLASS(Transient)`, GC-scanned via owner. Not a bug. |
| 6 | SourceObject doesn't replicate | `FindAbilityDefinitionByTag` registry fallback in `DataAbility.cpp` |
| 7 | Binary/cache files in git | Launcher artifacts removed, WebView2 cache gitignored |
| 8 | Server security | Server binds `127.0.0.1`, path traversal confined to Content |
| H3 | CheckCost client bypass | `AuraGameplayAbility.cpp` returns `true` for non-authoritative clients |
| C1 | WebView2 cache + launcher binaries in git | Cleaned in commits `2918713`/`144d96b`/`a47c56e` |
| C2 | Path traversal in Python server | `_confine_to_content` + loopback bind |
| C3 | SourceObject replication null on clients | Process-lifetime definition registry keyed by AbilityTag |
| L20 | CauseDamage vs ApplyDamage ASC access pattern | `CauseDamageNode` now uses `Ctx.ASC`, matching `ApplyDamageNode` |
| L15 | WorldContextObject never populated | Data-driven damage producers now set it to the ability avatar |
| L19 | ApplyDamage hardcodes bIsRadialDamage=false | Removed redundant radial-default assignments; the params struct owns the default and radial nodes set explicit values |
| L21 | Projectiles hardcode TargetASC=nullptr | Projectile nodes now seed the target ASC from `Ctx.CursorHit`; impact handling updates it to the collided actor |
| M8 | WaitForMontageEvent authored-tag validation | `WaitForMontageEvent` now validates the node tag against reflected `EventTag` metadata on the graph's loaded montage notifies |
| L13 | Hardcoded Python path in launcher | Launcher resolves `python.exe`, `python3.exe`, or `py.exe` through `PATH` with `SearchPathW` |
| L16 | ImportFactory CanReimport always false | XML import stores `SourceFilePath` and reimports into the existing definition object |
| L17 | MulticastGunFX fallback | Non-Aura avatars use generic local emitter/sound FX and actor location when no combat-socket interface is present |
| L22 | WaitForTargetData range validation | Client target data is validated for blocking hit, actor validity/self-targeting, and configurable maximum distance |

---

## Verification Notes
- Core architecture: ✅ IMPLEMENTED (Phases 1, 2, 5 complete)
- Build passes on UE 5.5.1
- Smoke test: `AuraAbilityGraphSmokeTest` (22 checks, 0 failures; verified 2026-08-04)
- Five active player abilities are ported: `FireBolt.xml`, `FireGun.xml`, `ArcaneShards.xml`, `FireBlast.xml`, and `Electrocute.xml` in `Content/AbilityDefinitions/`
- `RoleConfig.json` references the active player XML definitions; `EnemyAbilityConfig.json` references the four enemy definitions
