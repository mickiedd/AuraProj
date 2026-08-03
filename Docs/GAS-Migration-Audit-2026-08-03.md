# GAS Migration — Audit Findings (2026-08-03)

> Corrected audit of `Docs/GAS-Migration-TODOs.md` and `Docs/GAS-DataDriven-Rewrite-Plan-Pending.md`.
> Every claim below was re-verified against the current source tree on 2026-08-03.
> Related: `Docs/GAS-Migration-TODOs.md` (status tracker), `.claude/memory/gas-rewrite-pending.md` (memory status).

---

## Summary

A prior audit of the migration TODO and pending-plan produced 12 findings. Re-verification confirms the **code-level findings** (knockback direction, world-context population, montage-event callback wiring, and the low/medium known issues) and **two of the three internal TODO contradictions**. The audit is **wrong in substance** on the "missing `Effects.HitReact` tag" (the tag is natively registered) and **reversed on 3 of 4** stale-status claims (M7/M11/L14 are marked resolved in the TODO — correctly — while the Pending doc still lists them open).

The one confirmed functional bug (H2 knockback direction) has **since been fixed in the working tree but is not yet committed**.

---

## 1. Confirmed code issues

### 1.1 H2 — Knockback direction inconsistent → FIXED (uncommitted)

At audit time, `KnockbackForce` used `FVector::UpVector` in the projectile/spread nodes while direct-damage nodes used the target `Direction`:

| Node | Was | Line |
|---|---|---|
| `SpawnProjectileNode.cpp` | `UpVector * Mag` | `:130` |
| `SpawnProjectilesNode.cpp` | `UpVector * Mag` | `:165` |
| `HitscanTraceNode.cpp` | `UpVector * Mag` | `:96` |
| `ElectrocuteBeamNode.cpp` | `UpVector * Mag` | `:479` |
| `ApplyDamageNode.cpp` | `Direction * Mag` | `:67` |
| `EnemyMeleeDamageNode.cpp` | `Direction * Mag` | `:77` |

The working tree now uses `Direction`/`Forward` uniformly in all seven producers (including `SpawnShardsNode.cpp:244`), so knockback and death impulse are consistent. **These edits are uncommitted** (`git diff` shows 4 node files + the TODO modified). L18 (DeathImpulse vs Knockback mismatch within a node) is resolved by the same change.

**Caveat:** for projectiles the per-node value is functionally dead — on impact `AuraProjectile.cpp:264-274` overwrites both `DeathImpulse` and `KnockbackForce` with the projectile's own forward (knockback pitched 45°). Only the direct-damage nodes' values reach `LaunchCharacter` (`AuraAttributeSet.cpp:195-198`).

### 1.2 L15 — WorldContextObject nearly never populated

`FDamageEffectParams::WorldContextObject` (`AuraAbilityTypes.h:16`) defaults to `nullptr` and is only set by `EnemyMeleeDamageNode.cpp:63`. `ApplyDamageNode`, `HitscanTraceNode`, `SpawnProjectileNode`, `SpawnProjectilesNode`, and `CauseDamageNode` all leave it null.

### 1.3 M10 — WaitForMontageEvent bypasses the standard callback

`UWaitForMontageEventTask::OnEventReceived` (`WaitForMontageEventNode.cpp:116-126`) calls `DataAbility->AdvanceGraph(Success)` directly. The canonical `UAuraDataAbility::OnMontageEventReceived` (`DataAbility.cpp:411`) exists but is **never bound anywhere — dead code**. Fix: bind it in `WaitForMontageEventNode` (and drop the duplicate logic) or delete it.

### 1.4 M8 — only partially fixed

Empty/invalid `EventTag` is rejected (`WaitForMontageEventNode.cpp:44-52`), but the tag is not validated against the authored montage/AnimNotify metadata. Matches the TODO's own status.

### 1.5 L17 / L19 / L20 / L21 — confirmed open

- **L17** — `MulticastGunFXNode.cpp:59-67`: `AAuraCharacterBase` cast; non-Aura avatars get a warning and no FX fallback.
- **L19** — `ApplyDamageNode.cpp:69`: `bIsRadialDamage = false` hardcoded (no XML attribute).
- **L20** — `CauseDamageNode.cpp:45` uses `GetAbilitySystemComponentFromActorInfo()` vs ApplyDamage's `Ctx.ASC`.
- **L21** — `TargetAbilitySystemComponent = nullptr` at spawn in `SpawnProjectileNode.cpp:118` and `SpawnProjectilesNode.cpp:153`.

---

## 2. Document contradictions (in the TODO)

### 2.1 Verification step 5 vs 4.1 step 5 (Enemy UI metadata) — CONFIRMED

4.1 step 5 is checked: *"Confirm UI metadata is not applicable to AI-only abilities"* (correct — `AbilityInfo.json` has no enemy entries). Verification step 5 is unchecked and asks to *"Add enemy ability UI metadata to AbilityInfo.json."* These contradict. **Verification step 5 should be marked N/A.**

### 2.2 Verification step 4 references the wrong config file — CONFIRMED

Verification step 4 says *"New enemy roles defined in `RoleConfig.json`"*; enemy grants live in `EnemyAbilityConfig.json` (Elementalist / Warrior / Ranger). `RoleConfig.json` contains only the player roles (Aura, BungeeMan). 4.1 step 4 (checked) already names the correct file. **Verification step 4 should reference `EnemyAbilityConfig.json`.**

### 2.3 Step 6 wording vs native tag registration — DOC MISMATCH, not a bug

`Effects.HitReact` and `Abilities.Attack` are absent from `Config/DefaultGameplayTags.ini` but are **natively registered** in `AuraGameplayTags.cpp:258` / `:272` and `AuraAbilityGraphModule.cpp:1779-1780` (the module registers them before Aura's singleton to cover enemy CDO construction — the duplicate registration is idempotent). The tags exist at runtime; `SmokeTest_EnemyAbilityFiles` validates them. Only step 6's wording ("Add gameplay tags to `DefaultGameplayTags.ini`") does not match the implementation (native registration). **Reword step 6; no code change needed.**

---

## 3. Document drift between the three status files

| Item | TODO | Pending.md | memory | Code |
|---|---|---|---|---|
| H3 | open (High table) | **FIXED** (intentional) | FIXED | returns `true` on clients — by design |
| M7 | resolved | still open | unfixed | resolved (`PlayMontageNode.cpp:46-58`) |
| M11 | resolved | still open | unfixed | resolved centrally (`DataAbility.cpp:371`) |
| L14 | resolved | still open | **contradictory (both tables)** | resolved (`DataAbility.cpp:310`) |

- The TODO is the most accurate of the three for M7/M11/L14; `Pending.md` is stale.
- The "authoritative" memory file is internally contradictory: L14 appears in both its unfixed and FIXED tables.
- H3 should be moved out of the TODO's open High-priority table into a "verified intentional" note.

---

## 4. Stale unchecked boxes (work done, box unticked)

- **Enemy smoke tests exist but unchecked.** `SmokeTest_EnemyAbilityFiles` (`AuraAbilityGraphModule.cpp:1647`) validates all 4 enemy XMLs (tags, final node, curve rows) and `AuraEnemyAbilityMigrationTests.cpp` adds automation coverage. Verification item *"Smoke tests added for each new enemy ability XML"* should be checked.
- **4.3 items 2/4 are effectively done.** No runtime legacy GE references remain in C++; only smoke-test sample XML and a comment mention `GE_Damage`.
- **Build gate genuinely unverified.** *"Build passes after enemy ability additions"* is unchecked and only "compile-only validation passes" is claimed. Requires a real UBT build to close.

---

## 5. Where the prior audit was wrong

1. **"Missing `Effects.HitReact` tag"** — the tag is natively registered (see §2.3); not missing.
2. **"M7/M11/L14 marked FIXED in Pending"** — reversed; Pending lists them open while the TODO correctly marks them resolved (§3).
3. **"All 4 enemy XMLs reference `CT_Damage`"** — only the 3 damage-bearing XMLs have a `<damage>` element, and `CT_Damage` is a CurveTable **data asset** (data-driven, not a legacy Blueprint). The real legacy dependency is `EnemyRangedAttack.xml:11` → `ProjectileClass="BP_SlingshotRock.BP_SlingshotRock_C"`, which is also inconsistent with `EnemyFireBolt.xml` using native `ProjectileDefinition="fireBolt"`.
4. **"Smoke tests are fake" / "they have assertions"** — both overstated. 17 of 19 have real assertions; exactly two are log-only stubs: `SmokeTest_NodeRegistry` (`AuraAbilityGraphModule.cpp:109-138`) and `SmokeTest_SequenceExecution` (`:140-144`). TODO issue #4 is not fully resolved.
5. **"Phase 4.3 blocks Phase 6"** — unsupported. Phase 6 needs projectile actor classes (`AAuraProjectile` / `AAuraFireBall` already exist); removing legacy ability classes does not block it.
6. Minor: the legacy class is `UAuraFireGun`, not `UAuraGun`; the TODO also typos it as `UAauraFireBlast`.

---

## 6. Recommended actions (prioritized)

1. **Commit the H2/L18 knockback fix** already in the working tree (4 node files + TODO), or review it first.
2. **M10** — wire `DataAbility::OnMontageEventReceived` or delete it (it is dead code).
3. **Issue #4** — convert `NodeRegistry` + `SequenceExecution` smoke tests into real assertions.
4. **Docs** — fix TODO verification steps 4/5 (§2.1, §2.2); tick the enemy smoke-test box (§4); reword step 6 (§2.3); reconcile H3/M7/M11/L14 across TODO, Pending.md, and memory (§3); resolve the memory file's L14 self-contradiction.
5. **EnemyRangedAttack** — decide `ProjectileClass` (BP_SlingshotRock) vs native `ProjectileDefinition`, matching EnemyFireBolt.
6. **Build gate** — run a real build to close the last unverified item, then decide 4.3 remove-vs-repurpose for the 5 legacy classes (`UAuraFireBolt`, `UAuraFireGun`, `UArcaneShards`, `UAuraFireBlast`, `UElectrocute`).
