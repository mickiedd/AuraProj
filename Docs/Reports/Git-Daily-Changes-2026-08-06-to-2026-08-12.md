# AuraProj last-seven-days change reference - 2026-08-06 to 2026-08-12

This is a manually written supplement to the [full daily mind-map index](Git-Daily-Mind-Maps-2026-04-17-to-2026-08-11.md), shaped by the calendar-coverage mind map at [2026-08-11-daily-calendar-coverage.svg](Change-Archive/2026-08-11-daily-calendar-coverage.svg). It records every date in the seven-day window, including the quiet date, without inferring work that Git does not show.

## Audit frame

- Branch: `main` at `1c32b8e` (`Archive Git LFS snapshot size fix`).
- Window: 2026-08-06 through 2026-08-12, Asia/Shanghai calendar dates.
- Reachable history in the window: 17 commits.
- Active dates: 6. Quiet dates: 1.
- The 2026-08-11 merge is described as two source branches plus the merge/follow-up commits, so repository storage work is not confused with runtime behavior.
- This supplement was authored by inspecting `git log`, commit file lists, and the changed source/docs; no generation script produced the map.

| Date | Main commits | Daily result |
|---|---:|---|
| 2026-08-06 | 2 | Numeric monster debug spawning and structured docs folders |
| 2026-08-07 | 3 | NavMesh tooling, 20-day Role/Battle plan, and Day 1 smoke fixes |
| 2026-08-08 | 2 | Replicated combat identity and Day 2 network smoke |
| 2026-08-09 | 4 | Combat rules/state, smoke-mode policy, and damage inventory |
| 2026-08-10 | 1 | Shared damage boundary, attribution, and Day 4 runtime gate |
| 2026-08-11 | 5 | Safe avatar/beam fixes, runtime-asset LFS merge, and snapshot follow-up |
| 2026-08-12 | 0 | No commit on `main`; no daily diff or job is inferred |

## Boundary note for the 2026-08-11 illustration

If "last seven days" is read as the seven completed active dates ending on the referenced 2026-08-11 map, the window is 2026-08-05 through 2026-08-11. The 2026-08-05 boundary is already summarized in the August report and is restated here so that interpretation is complete:

- Commits: `779c09f` (`Add modular Electrocute beam nodes`) and `ff69b6e` (`Use GetDynamicSpecSourceTags() for ability specs`).
- Modular beam behavior was split into reusable initialization, targeting, visual, and damage operations; Electrocute XML and the original node were moved onto that composition path.
- Ability metadata consumers switched to `GetDynamicSpecSourceTags()` so runtime-generated ability specs preserve their source identity through ASC, data-ability, config, and test code.
- Validation surface: the August daily map, AuraAbilityGraph registration/smoke coverage, and GAS ability documentation were updated. This boundary note is context; the current seven-calendar-day totals above remain 7 dates and 17 commits.

## Detailed daily mind map

### 2026-08-06 - Debug spawning and documentation structure

Illustration: [2026-08-06-daily-change-reference.svg](Change-Archive/2026-08-06-daily-change-reference.svg).

Commits: `4c15782` (`Add runtime monster spawn by numeric ID`), `57fe4b6` (`Reorganize docs into structured subfolders`).

**Before -> after**

- Monster spawn table IDs changed from descriptive strings to numeric IDs `1` through `6`. The loader now reads numeric JSON values into `int32` rows.
- `AAuraGameModeBase::SpawnMonsterByIdAtLocation` became the authoritative lookup/spawn entry point. It loads the table when necessary, rejects non-authority calls, resolves the selected row, preserves level/class/scale/respawn data, and moves only the spawn location.
- The `AddMonster` cheat path became client-safe: cheat manager -> player controller request -> server RPC when needed -> authoritative GameMode spawn. The controller chooses a random location 150-250 units around the controlled pawn and reports success/failure to the client.
- `Docs/` gained explicit `Plans`, `Reference`, `Reports`, and `Tracking` categories plus `Docs/README.md`. Existing migration documents were moved into the category that describes how they should be used.

**Affected surface**

- Runtime/config: `Content/Config/MonsterSpawnTable.json`, `AuraGameModeBase`, `AuraPlayerController`, and `AuraCheatManager`.
- Documentation: migration plans, reports, references, tracking lists, and the docs index.

**Newcomer meaning**

Numeric IDs are a short debug command contract, not a replacement for the row's gameplay data. The GameMode remains the owner of spawning. The docs folders now distinguish intent before a reader opens a file.

### 2026-08-07 - Navigation fallback and the Role/Battle delivery program

Illustration: [2026-08-07-daily-change-reference.svg](Change-Archive/2026-08-07-daily-change-reference.svg).

Commits: `a26d241` (`Add NavMesh bounds tool, script, and fallback`), `f711913` (`Add Role/Battle system plan and 20-day index`), `098d7be` (`Day1 role-battle smoke & respawn attribute fixes`).

**Before -> after**

- Levels without navigation data could silently fail the BehaviorU wander move. `UAuraBehaviorUAgentComponent` now checks for a default NavMesh and disables pathfinding for a direct fallback move when none exists.
- A reusable editor menu command and `Scripts/add_navmesh_bounds_volume.py` now compute level bounds, add a `NavMeshBoundsVolume`, save the level, and trigger navigation building. The script also removes stale bounds volumes and skips absurd or zero-size actor bounds.
- The Role/Battle work became a named 20-day program with an implementation index and day-specific plans from baseline through cleanup/release.
- Day 1 gained a bounded runtime runner. Player-controller and character changes prevent persistent-ASC default attributes from stacking on each replacement pawn and initialize missing SetByCaller magnitudes to zero before selective assignment.

**Validation recorded by the day**

- The Day 1 report records an editor build pass, native Aura automation pass, AuraAbilityGraph smoke `22 passed, 0 failed`, successful StartupMap load, both damage directions, and two respawns with health `100/100` and mana `50/50`.
- The remaining visual/editor presentation check is explicitly kept separate from the headless gate in [Role-Battle-Baseline-2026-08-07.md](Role-Battle-Baseline-2026-08-07.md).

**Newcomer meaning**

This is the transition from broad migration work to a staged combat delivery. Navigation is made diagnosable and recoverable, while Day 1 establishes a repeatable baseline before identity and combat-rule changes.

### 2026-08-08 - Replicated combat identity

Illustration: [2026-08-08-daily-change-reference.svg](Change-Archive/2026-08-08-daily-change-reference.svg).

Commits: `2abcfcb` (`Expand Role-Battle Day 02/03/20 plans`), `8721069` (`Add CombatIdentity system and tests`).

**Before -> after**

- `FAuraCombatIdentity` defines faction, control type, combat profile, death policy, and explicit target/attack/damage/friendly-fire flags. Identity validity requires the four categorical gameplay tags.
- `UAuraCombatIdentityComponent` is replicated and server-owned. It validates server initialization, exposes generic actor lookup, logs client replication, and rate-limits missing-identity warnings.
- Player and enemy builders now resolve native gameplay tags after startup initialization, avoiding invalid values baked into early class default objects. `Combat.Unassigned` remains the deliberate transitional profile until the later role-schema days.
- `IsNotFriend` moved away from Player/Enemy actor tags to the bounded identity truth table. `BTService_FindNearestPlayer` now selects valid, targetable `Faction.Player` identities instead of relying on controller/tag fallback behavior.
- Day 2 gained three native identity tests and a repeatable two-process network runner; the plan documents were expanded for the identity, rules, and cleanup milestones.

**Validation recorded by the day**

- Focused Day 2 automation: `3/3` passed.
- Full Aura regression: `17/17` passed.
- AuraAbilityGraph smoke: `22/22` passed.
- Day 1 regression and Day 2 server/client replication smoke passed. The client received matching identity data through `OnRep_Identity`, and enemy behavior selected the replicated Player.
- Full evidence: [Role-Battle-Day-02-Combat-Identity-2026-08-08.md](Role-Battle-Day-02-Combat-Identity-2026-08-08.md).

**Newcomer meaning**

Identity answers what an actor represents. It is now an explicit, replicated combat boundary, not a display name, controller type, or incidental actor tag.

### 2026-08-09 - Authoritative combat rules/state and producer inventory

Illustration: [2026-08-09-daily-change-reference.svg](Change-Archive/2026-08-09-daily-change-reference.svg).

Commits: `da4b6e1`, `60d68d0`, `43f95bd`, `679fe3a`.

**Before -> after**

- The 20 day plans were expanded with executable scope, dependencies, validation, listen/dedicated requirements, and release notes. Day 2's network runner now has clearer bounded startup/assertion/teardown behavior.
- `EAuraCombatLifeState` and `UAuraCombatStateComponent` define the replicated authority-owned lifecycle `Alive -> Dying -> Dead -> Respawning -> Alive`. Repeated death transitions are idempotent; replacement pawns stay `Respawning` until initialization completes.
- `FAuraCombatRuleContext`, `FAuraCombatPolicySnapshot`, `FAuraCombatRules`, and structured rejection reasons provide one server-trusted answer for relationship, combat targeting, damage, and damage reception.
- The default relationship matrix is conservative: same-faction and protected targets are denied, Civilian combat is denied without an authoritative policy snapshot, and every non-Alive source or target is rejected.
- AI threat selection uses the candidate-to-observer direction and continues to target valid Player identities. Interaction remains a separate contract.
- `Role-Battle-Damage-Producer-Inventory.md` and its compile-time test enumerate projectiles, direct damage, beam, melee, hitscan, radial, and smoke producers and identify the expected shared boundary and authority side.

**Validation recorded by the day**

- Day 3 has eight native rule/state tests and the combined Day 1-3 suite has fourteen passing tests.
- Day 1 smoke and both Listen/Dedicated Day 2 and Day 3 network smokes are recorded as passing; the dedicated mode uses a bounded editor-server fallback when no cooked server map exists.
- The inventory test establishes the producer allowlist used by the next day's migration.

**Newcomer meaning**

Identity says what an actor is; rules and state say what that actor may do now. The important guard is default-deny on missing authority, identity, policy, targetability, damageability, or life state.

### 2026-08-10 - One safe, attributable damage boundary

Illustration: [2026-08-10-daily-change-reference.svg](Change-Archive/2026-08-10-daily-change-reference.svg).

Commit: `38763af` (`Harden damage boundary and preserve attribution`).

**Before -> after**

- `UAuraAbilitySystemLibrary::ApplyDamageEffect` now validates both ASCs, both avatar actors, authority, combat permission, and outgoing spec creation before applying damage. Rejected requests return an invalid context instead of dereferencing or applying through a bypass.
- `CauseDamage`, native projectile/fireball paths, and AuraAbilityGraph direct, beam, melee, hitscan, radial, and projectile producers are aligned to the shared `FDamageEffectParams` contract.
- `FAuraGameplayEffectContext` now carries ability tag, source role ID, source controller/player state, battle zone/event IDs, damage type, impulse, and knockback information.
- `ExecCalc_Damage` and `AuraAttributeSet` revalidate source/target authority, identity, relationship, and life state. Missing profile/table/curve data falls back to neutral coefficients rather than becoming an unsafe dereference.
- Periodic/debuff damage duplicates the custom context and rechecks current combat rules before each tick.
- `RunRoleBattleDay4DamageSmoke.ps1` and expanded Role/Battle automation turn the inventory into an executable gate.

**Validation recorded by the day**

- Day 4 automation: `10/10` passed.
- Listen and Dedicated damage smoke passed; the latter used the documented editor-server fallback in this checkout.
- Evidence and producer table: [Role-Battle-Damage-Producer-Inventory.md](Role-Battle-Damage-Producer-Inventory.md).

**Newcomer meaning**

When damage is wrong, follow the chain producer -> shared boundary -> combat context -> execution calculation -> attribute application. This day makes that chain explicit and rejects invalid requests at both important boundaries.

### 2026-08-11 - Beam safety, source fixes, and repository storage repair

Illustration: [2026-08-11-daily-change-reference.svg](Change-Archive/2026-08-11-daily-change-reference.svg).

Commits: `fb10b85`, `22573dc`, `5fe427a` (merge), `9588a92`, `1c32b8e`.

The date contains two different jobs. They are intentionally kept separate below.

#### Runtime code branch: `fb10b85`

- Added `GetSafeAvatarActor` and changed projectile, damage-library, execution-calculation, overlap, and test paths to use checked avatar access instead of assuming `AbilityActorInfo` is complete.
- Native actors use concrete `IsDead_Implementation` and `GetAvatar_Implementation` dispatch where generated interface wrappers can be ambiguous; Blueprint subclasses continue through the generated wrapper.
- Modular beam primary and chain targets now pass `FAuraCombatRules::CanDamage`. Friendly, protected, dead, self, or invalid actors can remain visual-only but cannot consume damage chain slots.
- Beam state now remembers effect parameters, search radius, maximum additional targets, and replacement policy. Pruning removes dead damage targets; eligible replacements are acquired and their Niagara visuals are spawned when configured.
- Test avatars now implement the combat interface methods needed to exercise actor validity, ability level, beam pruning, and target replacement.

#### Repository storage branch and merge: `22573dc`, `5fe427a`, `9588a92`, `1c32b8e`

- `.gitattributes` and `.gitignore` were expanded so required Unreal runtime assets are shared and `.uasset`/`.umap` files use Git LFS.
- The asset side contributed 2,747 changed paths: 2,745 runtime assets plus the two tracking files. This includes 2,252 `.uasset`, 18 `.umap`, and 475 snapshot paths.
- The merge combines the runtime code branch and the asset/LFS branch. It does not make LFS pointer files into gameplay behavior; it repairs repository transport for the runtime content that gameplay needs.
- The follow-up explicitly tracks `Content/BungeeMan/CR_BungeeMan.snapshot.json`, the 192,874,753-byte snapshot that exceeded GitHub's ordinary 100 MB blob limit.

**Validation recorded by the day**

- Beam/avatar regression smoke: `25 passed, 0 failed`.
- `git lfs fsck --objects`: `Git LFS fsck OK`; no ordinary Git blob remains over 100 MB in the migrated local history, and the full snapshot was restored through LFS checkout.
- Existing visual archive records: [Aura damage and modular beam fixes](Change-Archive/2026-08-11-aura-damage-beam-fixes.md) and [large snapshot flow](Change-Archive/2026-08-11-lfs-large-snapshot.md).

**Newcomer meaning**

There are two independent checks on this date: use safe avatar lookup and combat eligibility to understand runtime beam behavior; use Git LFS status to understand why a large Unreal asset may appear as a small pointer.

### 2026-08-12 - Explicit quiet-day record

Illustration: [2026-08-12-daily-change-reference.svg](Change-Archive/2026-08-12-daily-change-reference.svg).

`git log main` contains no commit dated 2026-08-12 at the time of this review. Therefore:

- No daily diff exists for this date.
- No implementation, fix, test result, or mind map is inferred.
- The date is still covered so a reader does not mistake the absence of a row for missing documentation.

## Seven-day narrative

The window moves through one coherent hardening arc:

1. Make debug spawning and documentation navigable.
2. Establish a staged Role/Battle plan and prove the Day 1 runtime baseline.
3. Replace incidental actor tags with replicated identity.
4. Add authoritative relationship/life-state rules and inventory every damage producer.
5. Force all damage through one attributable, authority-checked boundary.
6. Harden beam targeting and repair the storage path for the runtime assets.
7. Close the calendar with an explicit no-commit result for 2026-08-12.

The associated visual summary is [2026-08-12-seven-day-change-reference.svg](Change-Archive/2026-08-12-seven-day-change-reference.svg).

The seven date-level illustrations are archived in [2026-08-12-seven-daily-svg-references.md](Change-Archive/2026-08-12-seven-daily-svg-references.md).

## Review validation

- Date coverage: 7 of 7 calendar dates accounted for.
- Commit coverage: `git log main` reports 17 reachable commits in the window, grouped as `2 + 3 + 2 + 4 + 1 + 5 + 0` by date.
- Merge handling: the 2026-08-11 merge and its two parent jobs are described separately.
- Existing engine, automation, network, damage, beam, and LFS results are linked where they were recorded; this documentation-only supplement did not rerun gameplay binaries.
- The accompanying SVG is checked separately as XML in the archive record.
