# Data-Driven GAS Rewrite — Pending Phases & Known Issues

## Phases NOT Started (from Plan §10)

### Phase 3 — Port remaining player abilities ⬜ NOT STARTED
- ArcaneShards, FireBlast, Electrocute, Passives still use legacy BP abilities
- Need to create XML definitions for each
- Update `RoleConfig.json` `startupAbilityDefinitions` / `lmbAbilityDefinition` accordingly

### Phase 4 — Enemy abilities + cleanup ⬜ NOT STARTED
- Enemy abilities still use legacy BPs
- Legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk (dead code for DataAbility path)
- Remove unused GE Blueprint assets after migration complete

### Phase 6 (optional, future) — Projectile data-driven ⬜ NOT STARTED
- Move projectile mesh/FX/impact params into a data asset
- Eliminate `BP_FireBolt` / `BP_AuraBullet` / `BP_FireBall` Blueprint assets
- Extend XML schema or add separate projectile data asset

---

## Known Issues (from Plan §11) — Not Yet Fixed

| # | Issue | Location |
|---|-------|----------|
| 1 | **PlayMontage delegates not wired** — `OnInterrupted`/`OnCompleted` not bound; graph hangs if montage interrupted | `PlayMontageNode.cpp`, `DataAbility.cpp` |
| 2 | **Dead declarations** — `CreateNodeByClassName` (declared, not defined), `BuildAndExecuteGraph` (declared, not called) | `AbilityDefinition.h`, `DataAbility.h` |
| 3 | **Unused XML properties** — `TargetFromContext` parsed but ignored; `HitscanTraceNode::ScatterRadius` unused | Multiple node `.cpp` files |
| 4 | **Fake smoke tests** — `SmokeTest_NodeRegistry`, `SmokeTest_SequenceExecution` log only, don't assert | `AuraAbilityGraphModule.cpp` |
| 5 | **GC risk** — `Montage` and `DamageEffectClass` lack `UPROPERTY` on Transient UObject | `AbilityDefinition.h` |
| 6 | **SourceObject replication** — `FGameplayAbilitySpec::SourceObject` doesn't replicate; clients get null | `DataAbility.cpp`, grant path |
| 7 | **Binary/cache files in git** — 162 WebView2UserData cache files + `.exe/.pdb` committed | `AuraAbilityGraphLauncher/` |
| 8 | **Server security** — Python server binds `0.0.0.0`, path traversal in `/load` `/save` | `ability_graph_server.py` |

---

## Verification Notes
- Core architecture: ✅ IMPLEMENTED (Phases 1, 2, 5 complete)
- Build passes on UE 5.5.1 (per plan)
- Smoke test: `AuraAbilityGraph.SmokeTest` (7 tests)
- Two abilities fully ported: `FireGun.xml`, `FireBolt.xml`