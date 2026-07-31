# GAS → AuraAbilityGraph Migration TODOs

> Status tracker for migrating the remaining old-GAS-version (Blueprint/GE-UAsset) abilities to the new data-driven AuraAbilityGraph system.
> Last updated: 2026-07-31.
> Reference plan: `Docs/GAS-DataDriven-Rewrite-Plan-Pending.md`.

---

## Summary

| Status | Count | Items |
|---|---|---|
| ✅ Done | 5 | FireBolt, FireGun, ArcaneShards, FireBlast, Electrocute |
| ⬜ Not Started | 12 | Enemy abilities, passives, cleanup, known issues, optional Phase 6 |

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
- [x] 8/8 smoke tests pass (XML parsing, node registry, sequence execution, cooldown tag extraction, FireBolt migration, FireGun migration, RoleDefinition loading, Phase 3 file-graph validation, crash/lifecycle regression tests)

---

## ⬜ Phase 4 — Enemy Abilities + Cleanup (NOT STARTED)

### 4.1 Port Enemy Abilities to XML

| # | Legacy BP | Path | Difficulty |
|---|---|---|---|
| E1 | `GA_EnemyFireBolt` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — similar to FireBolt (projectile-based) |
| E2 | `GA_RangedAttack` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — projectile or hitscan |
| E3 | `GA_MeleeAttack` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Medium — melee, hitscan or instant |
| E4 | `GA_HitReact` | `Content/Blueprints/AbilitySystem/Aura/Abilities/Enemy/...` | Low — passive animation-only |

**Steps for each enemy ability:**
1. [ ] Create XML definition in `Content/AbilityDefinitions/` (e.g., `EnemyFireBolt.xml`)
2. [ ] Determine ability class hierarchy needed (likely `UAaurDamageGameplayAbility` or custom subclass)
3. [ ] If new node types needed (e.g., for melee reach, AI-specific targeting), add them to the plugin
4. [ ] Add enemy-specific `RoleConfig.json` entries (for each enemy type)
5. [ ] Add UI metadata to `AbilityInfo.json`
6. [ ] Add gameplay tags to `DefaultGameplayTags.ini`
7. [ ] Write test for XML parsing + graph validation
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

- [ ] Remove `UAuraFireBolt`, `UAuraGun`, `UArcaneShards`, `UAauraFireBlast`, `UElectrocute` C++ classes **or** repurpose them as the C++ function libraries called by `UAuraDataAbility` graph nodes (the C++ classes are currently kept as the class hierarchy for the traditional path — decide which path they belong to after full migration)
- [ ] Remove any `#include` / references to legacy GE UAsset paths in C++ code
- [ ] Remove any Blueprint-specific `UPROPERTY(EditDefaultsOnly)` that only served the legacy BP path
- [ ] Audit and remove any `GE_Cost_*` or `GE_Cooldown_*` references from C++ code that are now handled by shared C++ GEs

---

## ⬜ Phase 5 — Known Issues to Fix

These are the known issues from `GAS-DataDriven-Rewrite-Plan-Pending.md` that should be addressed:

### High Priority

| # | Issue | Description | Fix |
|---|---|---|---|
| H2 | Knockback force direction inconsistent | `ApplyDamageNode`, `HitscanTraceNode`, `SpawnProjectileNode` use Direction vs UpVector inconsistently | Standardize knockback direction to use `ToTarget` vector (Direction) or make it configurable in XML |
| H3 | CheckCost client bypass | `AuraGameplayAbility.cpp` returns `true` for non-authoritative clients | Verify this is intentional; add comment if so; consider adding a server-side validation callback |

### Medium Priority

| # | Issue | Description | Fix |
|---|---|---|---|
| 2 | Dead declarations | `CreateNodeByClassName`, `BuildAndExecuteGraph` declared but unused/dead | Remove dead declarations or implement their intended functionality |
| 3 | Unused XML properties | `TargetFromContext`, `ScatterRadius` in XML but not used by nodes | Either implement support for these properties or remove them from XML schema and docs |
| 4 | Fake smoke tests | Log-only tests with no assertions in `AuraAbilityGraphModule.cpp` | Convert to real unit tests with assertions |
| M7 | PlayMontage returns Success when no montage | `PlayMontageNode.cpp:29` — returns Success even when no montage is set | Add null-montage check that returns Failure instead |
| M8 | WaitForMontageEvent no EventTag validation | No validation that the requested EventTag matches the montage's actual tags | Add validation in `WaitForMontageEventNode::OnStart` |
| M9 | Two damage paths with different context setup | `ApplyDamageNode` vs `CauseDamageNode` set up `FDamageEffectParams` differently | Unify the context setup, or document the difference and make it intentional |
| M10 | WaitForMontageEvent bypasses OnMontageEventReceived | Directly calls `AdvanceGraph` instead of going through the standard callback | Fix to use `OnMontageEventReceived` path |
| M11 | WaitForTargetData no active-graph check | Can advance the graph after it's been deactivated | Add `bGraphActive` check |

### Low Priority

| # | Issue | Description | Fix |
|---|---|---|---|
| L13 | Hardcoded Python path in launcher | `AuraAbilityGraphLauncher.cpp:193` | Make path configurable |
| L14 | CooldownDuration doesn't scale from XML | `AbilityDefinition.cpp:138` — `CooldownDuration.Value` set from XML attr but doesn't use `GetValueAtLevel` | Use `FCooldownGameplayEffect` or scale via `CooldownDuration.GetStaticMagnitudeIfPossible` |
| L15 | WorldContextObject never populated | `AuraAbilityTypes.h:16` | Populate in `MakeDamageEffectParamsFromClassDefaults` and nodes |
| L16 | ImportFactory CanReimport always false | `AbilityDefinitionImportFactory.cpp` | Implement proper reimport support |
| L17 | MulticastGunFX no fallback for non-Aura characters | `MulticastGunFXNode.cpp` | Add null-check for `AAuraCharacterBase` cast |
| L18 | HitscanTrace DeathImpulse vs Knockback direction mismatch | `HitscanTraceNode.cpp` | Align directions with the standard pattern |
| L19 | ApplyDamage hardcodes `bIsRadialDamage=false` | `ApplyDamageNode.cpp:69` | Make configurable or remove the hardcoded override |
| L20 | CauseDamage vs ApplyDamage ASC access pattern | `CauseDamageNode.cpp:44` | Unify with ApplyDamage pattern |
| L21 | Projectiles hardcode `TargetASC=nullptr` | `SpawnProjectileNode.cpp`, `SpawnProjectilesNode.cpp` | Pass the actual TargetASC to projectiles |
| L22 | WaitForTargetData no OnStart validation | `WaitForTargetDataNode.cpp` | Add valid range/actor check on `OnStart` |

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

### Currently Eliminated by the C++ GE System (No Action Needed These Are Already Gone)

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
- [ ] New enemy roles defined in `RoleConfig.json` with correct `lmbAbilityDefinition` or `startupAbilityDefinitions`
- [ ] Enemy ability tags added to `DefaultGameplayTags.ini`
- [ ] Enemy ability UI metadata added to `AbilityInfo.json`
- [ ] Build passes after enemy ability additions
- [ ] Smoke tests added for each new enemy ability XML
- [ ] All old BP assets verified unused (no references in remaining code)

### Phase 5 Verification Steps (Issue Fixes)

- [ ] H2: Knockback direction consistent across all damage node types
- [ ] M7: `PlayMontage` returns Failure when no montage assigned
- [ ] M9: `ApplyDamageNode` and `CauseDamageNode` produce identical `FDamageEffectParams` context
- [ ] L14: `CooldownDuration` scales correctly with ability level from XML

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
- Projectile-based → `UAuraProjectileSpell` → `UAuraDamageGameplayAbility`
- Beam-based → `UAauraBeamSpell` → `UAauraDamageGameplayAbility`
- Direct damage → `UAauraDamageGameplayAbility` (no subclass needed if basic)
- Summon → `UAauraSummonAbility`
- Passive → `UAuraPassiveAbility` (not data-driven yet)

### Step 2: Write the XML definition
```xml
<ability name="NewAbility" abilityTag="Abilities.Category.NewAbility" 
         inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cost mana="15"/>
  <cooldown tag="Cooldown.Category.NewAbility" duration="8"/>
  <damage type="Damage.Fire" base="40" debuffChance="0.2" .../>
  <montage path="/Game/.../AM_NewAbility" eventTag="Event.Montage.NewAbility"/>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage"/>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.NewAbility"/>
      </node>
      <node class="SpawnProjectile">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileClass" value="/Game/.../BP_NewProjectile.BP_NewProjectile_C"/>
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
; Content/Config/AbilityInfo.json
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