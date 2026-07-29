# GAS Data-Driven Rewrite — Memory: Pending Phases

> Auto-generated from plan inspection on 2026-07-29.
> Core architecture (Phases 1, 2, 5) is **implemented**.
> This file records only what is **not yet started**.

---

## Phases Pending

| Phase | Description | Status |
|-------|-------------|--------|
| **3** | Port remaining player abilities (ArcaneShards, FireBlast, Electrocute, Passives) | ⬜ NOT STARTED |
| **4** | Enemy abilities + cleanup (legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk) | ⬜ NOT STARTED |
| **6** | Projectile data-driven (eliminate `BP_FireBolt` / `BP_AuraBullet` / `BP_FireBall`) | ⬜ OPTIONAL, NOT STARTED |

---

## Known Issues (Plan §11)

| # | Issue | Notes |
|---|-------|-------|
| 1 | PlayMontage delegates not wired | `OnInterrupted`/`OnCompleted` not bound; graph hangs if montage interrupted |
| 2 | Dead declarations | `CreateNodeByClassName`, `BuildAndExecuteGraph` declared but not defined/called |
| 3 | Unused XML properties | `TargetFromContext` ignored; `HitscanTraceNode::ScatterRadius` unused |
| 4 | Fake smoke tests | `SmokeTest_NodeRegistry`, `SmokeTest_SequenceExecution` log-only, no assertions |
| 5 | GC risk | `Montage` and `DamageEffectClass` lack `UPROPERTY` on Transient UObject |
| 6 | SourceObject replication | `FGameplayAbilitySpec::SourceObject` doesn't replicate; clients get null |
| 7 | Binary/cache in git | WebView2 cache + `.exe`/`.pdb` committed to repo |
| 8 | Server security | Python server binds `0.0.0.0`, path traversal in `/load` `/save` |

---

## Completed Milestones (for reference)

- Phase 1 — Framework ✅ (registry, nodes, driver, import factory)
- Phase 2 — MVP port: `FireGun` + `FireBolt` ✅
- Phase 5 — GE UAsset elimination ✅ (6 C++ GE classes + `GameplayEffects.json`)
