# GAS → AuraAbilityGraph Migration TODOs

> Status tracker for migrating the remaining old-GAS-version (Blueprint/GE-UAsset) abilities to the new data-driven AuraAbilityGraph system.
> Last updated: 2026-08-03.
> Reference plan: `Docs/GAS-DataDriven-Rewrite-Plan-Pending.md`.

---

## Summary

2026-08-02 decoupling update: active FireBolt/FireBlast/FireGun XML now uses cached native definitions from `ProjectileDefinitions.json`, and FireBall outbound/return behavior is native. The `AbilityInfo.json` runtime boundary is complete: active C++ and resaved UI/GameMode Blueprints no longer call `GetAbilityInfo()` or serialize a `DA_AbilityInfo` dependency. The legacy `DA_AbilityInfo` asset itself remains for cleanup/deletion after reference validation. Save/restore is tag-based and strict JSON validation is covered by automation tests. Projectile package deletion remains blocked: `BP_FireBolt` is referenced by `GA_FireBolt` and `GA_EnemyFireBolt`; `BP_FireBall` is referenced by `GA_FireBlast`. The native pickup migration is complete; legacy pickup packages were removed after reference, reload, automation, and cook gates.

Ordered continuation steps and acceptance gates: `Docs/Gameplay-Blueprint-Decoupling-Next-Moves.md`.

| Status | Count | Items |
|---|---|---|
| ✅ Done | 5 | FireBolt, FireGun, ArcaneShards, FireBlast, Electrocute |
| ⬜ Remaining | — | Enemy abilities, legacy asset cleanup, unresolved graph issues, optional Phase 6, and passives |

---

## ✅ Already Completed (Phase 3 — 2026-07-30)

- [x] 5 XML definitions in `Content/AbilityDefinitions/` (FireBolt, FireGun, ArcaneShards, FireBlast, Electrocute)
- [x] 3 new XML node types: `SpawnShards`, `ElectrocuteBeam`, `bSetReturnToOwner` property on `SpawnProjectiles`
- [x] `RoleConfig.json` updated — Aura role now has `startupAbilityDefinitions` (FireBlast, ArcaneShards, Electrocute) + `lmbAbilityDefinition` (FireBolt)
- [x] `RoleConfig.json` updated — BungeeMan `lmbAbilityDefinition` (FireGun)
- [x] `AbilityInfo.json` updated with all 5 data-driven abilities
- [x] `UAuraDataAbility` implements full runtime (ActivateAbility → graph execution → EndAbility)
- [x] `UAuraAbilityDefinition` XML parsing (`LoadFromXML`)
- [x] Build passes on UE 5.5.1
- [x] AuraAbilityGraph console smoke suite passes, including XML parsing, node registration, file-graph validation, projectile/beam behavior, and lifecycle regression checks (the suite contains more than eight named checks; it is separate from Automation tests)
- [x] AbilityInfo.json runtime boundary completed: runtime consumers migrated, tag-based save/restore implemented, strict metadata validation added, four active Blueprints resaved, and 5/5 focused metadata tests pass (9/9 total Aura automation tests)

---

## 🟨 Phase 4 — Enemy Abilities + Cleanup (IN PROGRESS)

Implementation update (2026-08-03): all four enemy definitions now exist and enemy startup grants load through `EnemyAbilityConfig.json`. Native graph nodes preserve combat-target selection, per-enemy tagged montages and sockets, montage-event timing, server-authoritative projectile/melee damage, and hit-react tag lifetime. Compile-only validation passes. Legacy packages remain until the active editor is closed and the reference/reload/automation gates can be run.

### 4.1 Port Enemy Abilities to XML

| # | Legacy BP | Path | Difficulty |
|---|---|---|---|
| E1 | `GA_EnemyFireBolt` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — similar to FireBolt (projectile-based) |
| E2 | `GA_RangedAttack` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — projectile or hitscan |
| E3 | `GA_MeleeAttack` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — melee, hitscan or instant |
| E4 | `GA_HitReact` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Low — passive animation-only |

**Steps for each enemy ability:**
1. [x] Create XML definitions in `Content/AbilityDefinitions/`
2. [x] Add tagged `UAuraDataAbility` subclasses for existing AI activation paths
3. [x] Add enemy montage, melee damage, and hit-react graph nodes
4. [x] Add `EnemyAbilityConfig.json` entries by `ECharacterClass` (enemy classes are separate from player `RoleConfig.json` roles)
5. [x] Confirm UI metadata is not applicable to AI-only abilities
6. [x] Add gameplay tags to `DefaultGameplayTags.ini`
7. [x] Write tests for XML parsing, graph structure, damage curves, and config validation
8. [ ] Remove legacy BP GA + GE assets

### 4.2 Remove Legacy Blueprint Assets (Dead Code)

After all abilities are migrated, remove these files from the repo:

**FireBolt/FireGun directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/`):
- [ ] `BP_FireBolt.uasset` (projectile BP)
- [ ] `GA_FireBolt.uasset` (legacy GA BP)
- [ ] `GA_FireGun.uasset` (legacy GA BP)
- [ ] `GE_Cooldown_FireBolt.uasset`
- [ ] `GE_Cooldown_FireGun.uasset`
- [ ] `GE_Cost_FireBolt.uasset`
- [ ] `BP_FireBolt.snapshot.json`
- [ ] `GA_FireBolt.snapshot.json`
- [ ] `GA_FireGun.snapshot.json`
- [ ] `GE_Cooldown_FireBolt.snapshot.json`
- [ ] `GE_Cooldown_FireGun.snapshot.json`
- [ ] `GE_Cost_FireBolt.snapshot.json`

**FireBlast directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBlast/`):
- [ ] `BP_FireBall.uasset` (projectile BP)
- [ ] `GA_FireBlast.uasset`
- [ ] `GE_Cooldown_FireBlast.uasset`
- [ ] `GE_Cost_FireBlast.uasset`
- [ ] All `.snapshot.json` files in directory

**ArcaneShards directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/Arcane/ArcaneShards/`):
- [ ] `GA_ArcaneShards.uasset`
- [ ] `BP_MagicCircle.uasset` (VFX actor BP)
- [ ] `BP_PointCollection.uasset` (VFX actor BP)
- [ ] `GE_Cooldown_ArcaneShards.uasset`
- [ ] `GE_Cost_ArcaneShards.uasset`
- [ ] All `.snapshot.json` files

**Electrocute directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/Lightning/`):
- [ ] `GA_Electrocute.uasset`
- [ ] `GE_Cooldown_Electrocute.uasset`
- [ ] `GE_Cost_Electrocute.uasset`
- [ ] All `.snapshot.json` files

**PassiveSpells directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/PassiveSpells/`):
- [ ] `GA_HaloOfProtection.uasset`
- [ ] `GA_LifeSiphon.uasset`
- [ ] `GA_ManaSiphon.uasset`
- [ ] All `.snapshot.json` files
- *Note: Passives are not yet data-driven (wrong archetype) — keep until Phase 5 or later*

**Passive_Startup directory** (`Content/Blueprints/AbilitySystem/Aura/Abilities/Passive_Startup/`):
- [ ] `GA_ListenForEvent.uasset`
- [ ] `GE_EventBasedEffect.uasset`
- [ ] All `.snapshot.json` files

### 4.3 Clean Up Source Code (Remove Old Compatibility Code)

- [ ] Remove `UAuraFireBolt`, `UAuraFireGun`, `UArcaneShards`, `UAuraFireBlast`, `UElectrocute` C++ classes **or** repurpose them as the C++ function libraries called by `UAuraDataAbility` graph nodes (the C++ classes are currently kept as the class hierarchy for the traditional path — decide which path they belong to after full migration)
- [x] Remove any `#include` / references to legacy GE UAsset paths in C++ code (no runtime legacy GE references remain; only smoke-test sample XML and a comment mention `GE_Damage`)
- [ ] Remove any Blueprint-specific `UPROPERTY(EditDefaultsOnly)` that only served the legacy BP path
- [x] Audit and remove any `GE_Cost_*` or `GE_Cooldown_*` references from C++ code that are now handled by shared C++ GEs (none remain in runtime C++)

---

## ⬜ Phase 5 — Known Issues to Fix

These are the known issues from `GAS-DataDriven-Rewrite-Plan-Pending.md` that should be addressed:

### High Priority

| # | Issue | Description | Fix |
|---|---|---|---|
| H2 | Knockback force direction inconsistent | Resolved: all damage nodes use their calculated target/projectile direction for knockback | Covered by `SmokeTest_DamageEffectParams` (2026-08-04) |

> **H3 — CheckCost client bypass: verified intentional.** `AuraGameplayAbility.cpp` returns `true` for non-authoritative clients by design (cost is enforced server-side). No action; retained here as a note rather than an open item.

### Medium Priority

Current-status note: M7, M8, M10, and M11 are resolved in the current implementation, and L14 is resolved by level-evaluating the XML cooldown duration. Issue #4 is resolved: `SmokeTest_NodeRegistry` and `SmokeTest_SequenceExecution` now carry real assertions (verified 2026-08-03). M8 now validates the requested tag against the loaded montage's authored AnimNotify metadata. M10's fix (2026-08-03): the wait task now routes through the canonical `OnMontageEventReceived`, so the wait path shares the `bGraphActive` guard with `OnMontageCompleted`/`OnTargetDataReady` instead of advancing the graph directly. Smoke suite is now 22 checks, 0 failed (2026-08-04).

| # | Issue | Description | Fix |
|---|---|---|---|
| 2 | Dead declarations **(resolved 2026-08-03)** | `CreateNodeByClassName`, `BuildAndExecuteGraph` were declared but unused/dead | Removed both declarations; the XML loader retains its file-local registry helper |
| 3 | Unused XML properties **(resolved 2026-08-03)** | `TargetFromContext`, `ScatterRadius` were parsed but ignored | Removed the dead properties from node schemas, XML definitions, editor metadata, smoke fixtures, and documentation |
| 4 | Fake smoke tests **(resolved 2026-08-03)** | Log-only tests with no assertions in `AuraAbilityGraphModule.cpp` | `SmokeTest_NodeRegistry` now validates all 17 concrete registrations and rejects unknown names; `SmokeTest_SequenceExecution` parses and executes a real two-child sequence |
| M7 | PlayMontage missing montage behavior | Resolved: configured load failures return Failure; an intentionally empty montage remains a successful no-op | Keep regression coverage |
| M8 | WaitForMontageEvent no authored-tag validation **(resolved 2026-08-03)** | The node rejected an empty/invalid `EventTag`, but did not verify that the tag existed on the authored montage/AnimNotify | `WaitForMontageEvent` now finds the graph's `PlayMontage` node and validates the tag against reflected `EventTag` metadata on its montage notifies |
| M9 | Two damage paths with different context setup **(resolved 2026-08-03)** | `ApplyDamageNode` vs `CauseDamageNode` set up `FDamageEffectParams` differently | `CauseDamageNode` now matches `ApplyDamageNode`: `Ctx.ASC`, identical params, and shared `ApplyDamageEffect` path |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Resolved: the wait task now routes through `UAuraDataAbility::OnMontageEventReceived`, which applies the `bGraphActive` guard before advancing (verified 2026-08-03, smoke 22/22) | Keep regression coverage |
| M11 | WaitForTargetData callback lifecycle | Resolved centrally: `OnTargetDataReady`, `AdvanceGraph`, and montage callbacks ignore events after `bGraphActive` becomes false | Keep regression coverage |

### Low Priority

Current-status note: L13, L16, and L17 are resolved in the current implementation. L15, L19, L20, and L21 remain resolved, and L22 now validates client target data against the configured avatar range. Regression coverage added 2026-08-04: `SmokeTest_TargetDataValidation` (L22) and `SmokeTest_DamageEffectParams` (H2/L15/L19/L21); the seven damage nodes now share `UAuraAbilityDefinition::BuildDamageEffectParams`.

| # | Issue | Description | Fix |
|---|---|---|---|
| L13 | Hardcoded Python path in launcher **(resolved 2026-08-03)** | Launcher used machine-specific absolute Python paths | Launcher now resolves `python.exe`, `python3.exe`, or `py.exe` through Windows `PATH` with `SearchPathW` |
| L14 | CooldownDuration scaling | Resolved: XML duration is evaluated with `CooldownDuration.GetValueAtLevel(GetAbilityLevel())` before the cooldown spec is applied | Keep regression coverage |
| L15 | WorldContextObject never populated **(resolved 2026-08-03)** | Damage-param producers left it null | Data-driven damage nodes now set it to `Ctx.AvatarActor` |
| L16 | ImportFactory CanReimport always false **(resolved 2026-08-03)** | XML import did not retain a source path and always returned false | Import stores `SourceFilePath`, exposes it through `CanReimport`, and reloads/parses the same object in `Reimport` |
| L17 | MulticastGunFX lacks fallback for non-Aura characters **(resolved 2026-08-03)** | Non-`AAuraCharacterBase` avatars received no FX | The node now uses a generic local emitter/sound fallback and safely uses actor location when no combat-socket interface is present |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | Resolved: hitscan knockback now follows its directional death impulse | Keep regression coverage |
| L19 | ApplyDamage hardcodes `bIsRadialDamage=false` **(resolved 2026-08-03)** | `ApplyDamageNode.cpp` redundantly assigned the radial defaults | Removed the redundant assignments; `FDamageEffectParams` owns the non-radial default |
| L20 | CauseDamage vs ApplyDamage ASC access pattern **(resolved 2026-08-03)** | `CauseDamageNode.cpp` used a different source ASC | `CauseDamageNode` now uses `Ctx.ASC` and the shared `FDamageEffectParams` path |
| L21 | Projectiles hardcode `TargetASC=nullptr` **(resolved 2026-08-03)** | Projectile params discarded the known cursor target ASC | Projectile nodes now seed `TargetAbilitySystemComponent` from `Ctx.CursorHit` |
| L22 | WaitForTargetData range validation **(resolved 2026-08-03)** | Client-supplied target data was consumed without validating its hit, actor, or distance | `WaitForTargetData` now validates blocking hit data, rejects self/invalid actors, and enforces configurable `MaxTargetDistance` (default 10,000) before advancing |

---

## ⬜ Phase 6 (Optional) — Projectile Data-Driven

| # | Item | Details |
|---|---|---|
| P1 | Move projectile mesh/FX/impact params into a data asset | Currently `BP_FireBolt` and `BP_FireBall` are heavily BP-bound (mesh, FX, collision, overlap handlers) |
| P2 | Eliminate `BP_FireBolt` / `BP_FireBall` Blueprint assets | Replace with C++ projectile class + data asset config |
| P3 | `BP_AuraBullet` already removed | This was done; remaining projectile BPs need replacement |
| P4 | High effort — projectile actors are heavily BP-bound | Marked optional; requires significant C++ work to replace mesh/FX/collision Blueprint logic |

---

## ⬜ Phase 7 — Passives Data-Driven (Future)

Currently passives (`HaloOfProtection`, `LifeSiphon`, `ManaSiphon`) are left as legacy because they use a passive GameplayEffect archetype, not an action graph.

**Options:**
1. **Keep as legacy** — Passives don't need the full action graph; they can remain as Blueprint GEs or C++ passives
2. **Port to XML with a passive node type** — Add `SpawnPassiveEffect` or `ApplyGameplayEffect` node to the AuraAbilityGraph system
3. **Move to `GameplayEffects.json`** — If passives are purely GE-driven (no action graph), they can be configured in `GameplayEffects.json` instead of XML

**Recommended approach:** Option 3 for simple passives (pure GE application), Option 2 for passives that need conditional logic or targeting.

---

## ⬜ Cleanup: GE Blueprint UAssets

### Replaced at Runtime by the C++ GE System

These assets are no longer runtime dependencies, but replacement does not mean the corresponding legacy packages have already been deleted from the repository. The packages listed in Section 4.2 remain cleanup candidates until reference and reload gates pass.

The following GE Blueprint UAssets are already dead code — the C++ GE classes replaced them:

- `GE_Damage` (if it existed as a BP) → Replaced by `UAuraDamageGameplayEffect`
- `GE_Cost_*` (if they existed) → Replaced by `UAuraManaCostGameplayEffect`
- `GE_Cooldown_*` (if they existed) → Replaced by `UAuraCooldownGameplayEffect`
- `PrimaryAttributes_SetByCaller` BP → Replaced by `UAuraAttributeGameplayEffect`
- `SecondaryAttributes` BP → Replaced by `UAuraAttributeGameplayEffect`
- `VitalAttributes` BP → Replaced by `UAuraAttributeGameplayEffect`

### Still Need Removal (Legacy BPs That Haven't Been Cleaned Up)

The legacy `.uasset` and `.snapshot.json` files listed in Section 4.2 above still exist on disk. They serve no runtime purpose since the data-driven path now works, but they should be removed for:
- Version control cleanliness
- Avoiding confusion for new developers
- Preventing accidental use of legacy assets

---

## ⬜ Verification & Testing

### Phase 4 Verification Steps

- [ ] All 4 enemy ability XMLs parse correctly (`UAuraAbilityDefinition::LoadFromXML` returns `true`)
- [ ] Enemy ability graph structures validated (correct node types, valid property values)
- [ ] New enemy roles defined in `EnemyAbilityConfig.json` with correct grants by `ECharacterClass` (player roles live separately in `RoleConfig.json`)
- [x] Enemy ability tags added to `DefaultGameplayTags.ini` (`Abilities.Melee`, `Abilities.Ranged`; `Effects.HitReact` and `Abilities.Attack` are natively registered in `AuraGameplayTags.cpp` rather than the ini)
- [ ] Enemy ability UI metadata added to `AbilityInfo.json` — **N/A**: UI metadata is not applicable to AI-only abilities (see 4.1 step 5)
- [ ] Build passes after enemy ability additions
- [x] Smoke tests added for each new enemy ability XML (`SmokeTest_EnemyAbilityFiles` in `AuraAbilityGraphModule.cpp`)
- [ ] All old BP assets verified unused (no references in remaining code)

### Phase 5 Verification Steps (Issue Fixes)

- [x] H2: Knockback direction consistent across all damage node types
- [x] #4: Node registry and sequence smoke tests use real assertions
- [x] #3: Unused XML properties removed from runtime/editor schemas and authored definitions
- [x] M7: `PlayMontage` handles missing montage configuration safely (configured load failures return Failure)
- [x] M9: `ApplyDamageNode` and `CauseDamageNode` produce identical `FDamageEffectParams` context
- [x] L20: `CauseDamageNode` uses the same `Ctx.ASC` source pattern as `ApplyDamageNode`
- [x] L15: Damage-param producers populate `WorldContextObject`
- [x] L19: `ApplyDamageNode` no longer redundantly hardcodes radial defaults
- [x] L21: Projectile damage params carry the known cursor target ASC
- [x] L14: `CooldownDuration` scales correctly with ability level from XML

### Phase 6 Verification Steps (Optional)

- [ ] Projectile data asset schema defined
- [ ] NPCH projectile BP replaced with C++ class + data asset
- [ ] All VFX/mesh/collision params configurable via data asset
- [ ] Remaining legacy projectile BPs removed

---

## ⬜ Documentation Updates

- [ ] Update `Docs/GAS-DataDriven-Rewrite-Plan.md` to reflect completed items and remaining TODOs
- [ ] Update `Docs/GAS-DataDriven-Rewrite-Plan-Pending.md` with current status
- [ ] Update `Docs/GAS-Abilities-Documentation.md` when new abilities are added
- [ ] Add inline code comments in `DataAbility.cpp` for complex lifecycle logic (cancel propagation, re-entrancy guard)
- [ ] Add XML schema reference doc (e.g., `Docs/AbilityXML-Schema.md`) for new developers

---

## Dependency Graph

```
Phase 3 (DONE)
 │
 ├──► Phase 4 (Enemy Abilities + Cleanup)
 │    │
 │    ├──► E1-E4: Port enemy abilities to XML (depends on Phase 3)
 │    ├──► 4.2: Remove legacy BP assets (depends on E1-E4)
 │    └──► 4.3: Clean up source code (depends on 4.2)
 │
 ├──► Phase 5 (Known Issues)
 │    ├──► H2, H3: High priority fixes (independent of Phase 4)
 │    ├──► M7-M11: Medium priority fixes (independent of Phase 4)
 │    └──► L13-L22: Low priority fixes (independent of Phase 4)
 │
 ├──► Phase 6 (Optional — Projectile Data-Driven)
 │    └──► P1-P4: Remove projectile BPs (depends on Phase 4+5)
 │
 └──► Phase 7 (Optional — Passives Data-Driven)
      └──► Decide approach, then implement

All Phases ──► Phase 8: Verification & Documentation
```

---

## Quick-Start: How to Add a New Ability (Data-Driven)

When adding a new ability (e.g., an enemy ability or a new player spell), follow these steps:

### Step 1: Determine the C++ class hierarchy
- Active XML abilities use `UAuraDataAbility` and registered graph nodes. The legacy spell/passive classes below describe the traditional path and should not be selected for a new XML definition unless a deliberate compatibility path is required.
- Projectile-based → `UAuraProjectileSpell` → `UAuraDamageGameplayAbility`
- Beam-based → `UAuraBeamSpell` → `UAuraDamageGameplayAbility`
- Direct damage → `UAuraDamageGameplayAbility` (no subclass needed if basic)
- Summon → `UAuraSummonAbility`
- Passive → `UAuraPassiveAbility` (not data-driven yet)

### Step 2: Write the XML definition
Use node properties for montage and projectile configuration. The XML loader does not treat a standalone `<montage>` element as the active graph configuration. Prefer `ProjectileDefinition` for native projectile configuration rather than a Blueprint-generated projectile class.
```xml
<ability name="NewAbility" abilityTag="Abilities.Category.NewAbility" 
         inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cost mana="15"/>
  <cooldown tag="Cooldown.Category.NewAbility" duration="8"/>
  <damage type="Damage.Fire" base="40" debuffChance="0.2" .../>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage">
        <property name="Montage" value="/Game/.../AM_NewAbility.AM_NewAbility"/>
      </node>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.NewAbility"/>
      </node>
      <node class="SpawnProjectile">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileDefinition" value="newProjectile"/>
      </node>
    </node>
  </graph>
</ability>
```

### Step 3: Add gameplay tag
```ini
; Config/DefaultGameplayTags.ini
+GameplayTagList=(Tag="Abilities.Category.NewAbility")
+GameplayTagList=(Tag="Cooldown.Category.NewAbility")
```

### Step 4: Add UI metadata
```json
// Content/Config/AbilityInfo.json (documentation label; omit this line from actual JSON)
{
  "abilityTag": "Abilities.Category.NewAbility",
  "icon": "/Game/UI/Icons/T_NewAbility.T_NewAbility",
  "backgroundMaterial": "/Game/UI/Materials/M_AbilityBackground_Fire.M_AbilityBackground_Fire",
  "levelRequirement": 1
}
```

### Step 5: Wire into RoleConfig.json
```json
{
  "role": "Aura",
  "startupAbilityDefinitions": [
    "/Game/AbilityDefinitions/NewAbility.xml"
  ]
}
```

### Step 6: Test
- Verify XML parses correctly (check `LoadFromXML` returns `true`)
- Verify all node types exist in the registry
- Add a smoke test in `AuraAbilityGraphModule.cpp`
- Run the full test suite
- Build and pass
