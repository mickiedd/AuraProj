# Crunch Character/Pawn Behavior Migration — Completion Plan

Date: 2026-09-02  
Status: Implementation in progress; Days 01–02 complete, Day 03 movement-preserving listen evidence reviewed and provisional pending full terminal closure, Day 04 presentation materialized under validation, Days 05–08 native skill and role integration implemented with focused gates passing, Day 09 package cook and game startup passed but matching packaged host/client topology remains blocked by the missing staged AuraServer.exe, and Day 10 independent sign-off remains open
Source project: `C:\Works\Crunch-master` (read-only reference)  
Target project: `C:\Git\AuraProj` (Unreal Engine 5.5)

## Mission

Finish the Crunch migration as a real AuraProj character rather than a selectable Aura-shaped combo prototype. The completed slice must preserve AuraProj's role-owned, server-authoritative GAS and persistence contracts while reproducing Crunch's distinctive avatar, locomotion-compatible presentation, LMB combo, UpperCut, Dash, GroundBlast, and Tornado behaviors.

The work is complete only when a matching packaged listen host and remote client select Crunch through the normal Login flow, enter gameplay, visibly use every migrated skill against valid and invalid targets, survive pawn replacement/reconnect, and produce reviewable machine and visual evidence. A package that stops at Login or Loading, an `OutdatedClient` result, or automation-only proof is not completion.

## Starting truth

- `main` is still at `d4f80e6`; the migration is a large uncommitted working-tree stack and must be isolated from unrelated pending changes before edits continue.
- `Content/Config/RoleConfig.json` exposes a selectable Crunch role. Crunch owns `/Script/Aura.AuraMeleeAttack` on `InputTag.LMB`; Aura keeps `FireBolt.xml`.
- `UAuraMeleeAttack` implements the four-section combo and now targets the target-owned `AM_CrunchComboV4` montage on `Crunch_SkeletonV4`; the target-fixture runtime replay remains a final validation gate.
- Crunch now has target-owned `SM_CrunchV4` geometry and `ABP_Crunch_AuraV4` with a `DefaultSlot` graph; visual calibration and packaged listen proof remain open.
- The source Crunch hero also owns UpperCut, Dash, GroundBlast, and Tornado behavior. None is granted by AuraProj's Crunch role.
- The latest local RoleBattle record is 237/237 and a Win64 Development package was produced. A source-built matching `AuraServer` and cooked WindowsServer stage now provide a dedicated ComboFull proof; matching packaged normal-flow Crunch proof still does not exist.
- Review findings F1, F2, F4, and F5 in `Docs/Reports/Crunch-Combo-Pending-Diff-Review-2026-09-01.md` remain visible in source. F3's old “phantom RoleConfig change” diagnosis is superseded by the genuine Crunch role entry and must not be applied mechanically.
- The installed UE 5.5 distribution cannot build a Server target, but the source checkout now provides a matching `AuraServer` and cooked WindowsServer stage. Dedicated ComboFull is validated only on that source-built staged lane; launcher/distribution parity and movement-preserving listen evidence remain separate gates.

## Frozen product and input contract

The supported Crunch role uses this fixed input map:

| Input | Ability tag | Player behavior |
| --- | --- | --- |
| `InputTag.LMB` | `Abilities.Melee.CrunchCombo` | Four-hit authored combo with authoritative damage windows. |
| `InputTag.1` | `Abilities.Melee.CrunchUppercut` | Launch self and valid nearby targets, then permit authored airborne follow-up sections. |
| `InputTag.2` | `Abilities.Melee.CrunchDash` | Predicted forward dash; authority applies bounded contact damage/push once per target. |
| `InputTag.3` | `Abilities.Melee.CrunchGroundBlast` | Owner-targeted ground selection followed by authoritative radial damage/push and cue. |
| `InputTag.4` | `Abilities.Melee.CrunchTornado` | Bounded four-second spin; authority applies damage/push only at authored hit events. |

The source assets are semantic references, not runtime dependencies. AuraProj will not load `/Game/Characters/Crunch/...` from the source checkout, copy Crunch C++ classes, or import source GameplayEffect/target-actor authority. Source Blueprint defaults, montage timing, mesh/skeleton dependencies, push values, costs, cooldowns, ranges, and effect names are exported on Day 01 into a hashed manifest. Later days translate that frozen manifest through AuraProj's shared damage, relationship, role-grant, input, HUD, and persistence boundaries.

## Authority and state contract

- The server decides activation acceptance, cost/cooldown commit, combo successor acceptance, target membership, relationship/life/range filters, damage, launch/push, and ability termination.
- Clients may predict montage, locomotion presentation, targeting preview, and input responsiveness. Client target data is a request; the server traces or revalidates it against its current world state before committing gameplay.
- Each activation has a server-issued/validated activation identity and per-target hit set. Retries, overlap jitter, duplicate montage events, and late packets cannot apply an effect twice.
- Standalone and listen authority use authored montage events. Only `NM_DedicatedServer` may use the deterministic fallback timeline. Test harness limitations must not change shipping behavior.
- Cost/cooldown follows prepare → validate target/commit point → commit → publish. Cancel before commit consumes nothing; failure after a successful gameplay commit does not refund or duplicate on retry.
- Role-owned specs remain in the persistent PlayerState ASC ledger. Pawn replacement/reconnect must yield exactly one spec for each of the five Crunch inputs and zero Aura/BungeeMan input collisions.
- Death, role teardown, disconnect, montage interruption, and ability cancellation clear timers, tasks, targeting actors, movement modifiers, active effects, queued successors, and local cues within one frame plus the documented network propagation allowance.
- Role presentation is driven by the replicated authorized role and a matching packaged config version. Clients never choose mesh, capsule, movement, ability class, effect, damage, or sockets.

## Shared execution and evidence contract

Day 01 adds one planned runner, `RunCrunchMigration.ps1`, with `-Stage Fast|Network|Package|Final`, `-Scenario`, `-RunId`, and explicit engine/package paths. `RunCrunchMigrationExportPreflight.ps1` and `RunCrunchSourceEditorPreflight.ps1` are its fail-closed source-evidence gates. They write under:

`Saved/Reports/CrunchMigration/<SourceRevision>/<RunId>/`

Every artifact records source revision, source-asset manifest hash, target revision, RoleConfig/AbilityInfo hashes, build configuration, topology, process IDs, start/end UTC, timeout, exit code, and log paths. No runner selects “latest” evidence implicitly.

Default bounds are: build 30 minutes, focused automation 15 minutes, package 45 minutes, server readiness 90 seconds, client readiness 90 seconds, gameplay scenario 180 seconds, and teardown 20 seconds. Timeout is a failure, returns nonzero, captures the last 200 log lines, and terminates only child processes started by that RunId. Success requires zero crash, fatal, assertion, ensure, stale-asset, missing-package, and unresolved-role-config signatures unless an exact expected-warning allowlist is checked in and counted.

After each implementation day: run the focused gate, create the required Markdown/SVG change archive, send the bounded code/evidence packet through `in-app-chatgpt-handoff`, apply all demonstrated findings, rerun affected gates, and only then advance. A handoff failure is recorded with attempts and local fallback review; it never becomes an attributed external approval.

## Dependency order

| Day | Milestone | Depends on | Main output |
| --- | --- | --- | --- |
| 01 | Freeze source truth and isolate the candidate | Current workspace | Hashed scope/asset/ownership manifests and repeatable runner. |
| 02 | Repair pending combo review debt | Day 01 | Trustworthy tag/cue/close/activation tests. |
| 03 | Unify authored combo timing | Day 02 | Listen uses authored events; deterministic boundary smokes pass. |
| 04 | Migrate Crunch avatar and animation presentation | Day 03 | Crunch mesh, skeleton, ABP, geometry, sockets, and authored combo montage. |
| 05 | Add shared Crunch ability boundary and UpperCut | Day 04 | Native translated UpperCut with airborne chain proof. |
| 06 | Implement Dash | Day 05 | Prediction-safe dash with server-owned contact results and cleanup. |
| 07 | Implement GroundBlast and Tornado | Day 06 | Server-revalidated area skill and bounded channeled spin. |
| 08 | Integrate the five-skill role, HUD, and lifecycle | Day 07 | Collision-free role loadout across replacement/reconnect. |
| 09 | Run matching packaged normal-flow matrix | Day 08 | In-world host/client behavior and visual evidence for every skill. |
| 10 | Independent review and honest sign-off | Day 09 | Final manifest, limitations, review resolution, and completion decision. |

## Day 01 — Freeze source truth and isolate the candidate

### Player outcome

No player-facing change. Establish one reproducible baseline so later work cannot accidentally mix unrelated working-tree edits, stale packages, guessed tuning, or incomplete source dependencies.

### Exact change surfaces

- New: `Docs/Plans/Crunch-Migration/crunch-migration-scope.json`.
- New: `Docs/Plans/Crunch-Migration/crunch-source-ability-contract.json`.
- New: `Docs/Plans/Crunch-Migration/crunch-source-asset-manifest.json`.
- New: `Docs/Reports/Crunch-Migration-Change-Ownership-2026-09-02.md`.
- New: `RunCrunchMigration.ps1`, `RunCrunchMigrationExportPreflight.ps1`, `RunCrunchSourceEditorPreflight.ps1`, and `Scripts/Tests/test_crunch_migration_runner.py`.
- Read-only source roots: `C:/Works/Crunch-master/Source/Crunch/Private/GAS`, `Content/Characters/Crunch`, and `Content/ParagonCrunch/Characters/Heroes/Crunch`.

### Data and authority contract

The scope manifest fixes the five inputs above, supported Win64 standalone/listen topologies, dedicated `BLOCKED` classification, required normal-flow journey, and anti-goals. The source ability contract records actual Blueprint/CDO defaults and montage event times for Combo, UpperCut, Dash, GroundBlast, and Tornado. Each row includes source object path, class, input, cost, cooldown, duration, range/radius, damage/push definition, montage/section/event data, gameplay cues, and SHA-256. Missing/unreadable fields block downstream implementation; zero/default is accepted only when the source object explicitly contains it.

### Numbered implementation steps

1. Classify every pending file as Crunch-owned, prerequisite/shared, unrelated, or generated evidence; record overlaps without reverting user work.
2. Export source Asset Registry dependencies and Blueprint/CDO properties using a read-only commandlet or editor Python script; do not parse opaque `.uasset` bytes as authoritative values.
3. Hash the source files and export results; freeze the target package paths and the five-input mapping.
4. Record the current target build, discovered test counts, RoleConfig/AbilityInfo hashes, package version, and current known failures.
5. Implement the bounded runner with child-process ownership, explicit result schema, no implicit package selection, nonzero failure propagation, and `-WhatIf`/syntax coverage.
6. Run the required independent scope/architecture review and apply findings to the manifests before Day 02.

### Named tests and commands

- `CrunchMigrationRunner.ContractPathsStayUnderSaved`
- `CrunchMigrationRunner.TimeoutReturnsNonZero`
- `CrunchMigrationRunner.KillsOnlyOwnedChildren`
- `CrunchMigrationRunner.RejectsMismatchedRevisionOrManifest`
- `CrunchMigration.SourceEditorPreflight`
- `CrunchMigration.SourceExportPreflight`
- `Aura.Migration.Crunch.ScopeManifest`
- `Aura.Migration.Crunch.SourceAssetManifest`

```powershell
& C:/Git/UE_5.5/Engine/Build/BatchFiles/Build.bat AuraEditor Win64 Development C:/Git/AuraProj/Aura.uproject -waitmutex
python -m unittest Scripts.Tests.test_crunch_migration_runner
./RunCrunchMigration.ps1 -Stage Fast -Scenario Baseline -RunId crunch-d01-baseline
```

### Fixtures, failure semantics, artifacts, and gate

Use the untouched source checkout, current AuraProj workspace, and a no-editor-process preflight. Produce the three manifests, ownership report, baseline result JSON, hashes, and review response. Fail on an active editor locking changed binaries/assets, missing source project, unresolved asset reference, dirty-file ownership ambiguity, output outside `Saved/Reports/CrunchMigration`, or baseline command timeout. Day 01 passes only when every current change has an owner, every source behavior field is frozen or explicitly blocks, and another developer can reproduce the baseline without modifying Crunch-master.

### Defer / anti-goals

No gameplay edits, asset copying, tuning reinterpretation, cleanup of unrelated pending changes, or commit/rebase operation.

## Day 02 — Repair pending combo review debt

### Player outcome

The existing LMB behavior should look unchanged, but its evidence becomes meaningful and future regressions can no longer hide behind duplicate tags, source-text assertions, or fabricated counters.

### Exact change surfaces

- `Config/DefaultGameplayTags.ini`.
- `Source/Aura/Private/AuraGameplayTags.cpp` and `Public/AuraGameplayTags.h`.
- `Source/Aura/Private/Tests/AuraCrunchComboRuntimeTests.cpp`.
- `Source/Aura/Private/AbilitySystem/Abilities/AuraMeleeAttack.cpp` and public header.
- Development-only probe logging in `Source/Aura/Private/Player/AuraPlayerController.cpp` and public header.

### Data and authority contract

`GameplayCue.MeleeImpact` has one registration owner: config. Runtime tests request it from `UGameplayTagsManager` and observe cue dispatch through a real target ASC/listener. `TestCloseEventCount` counts authored close events only; implicit teardown closure has a separate counter. `ActivateAbility` calls its base contract and ends fail-closed when activation lacks authority/prediction permission.

### Numbered implementation steps

1. Remove the duplicate native cue registration while retaining the config tag and typo redirect.
2. Replace the `IsValid()` member check with `RequestGameplayTag(..., false)` registration proof.
3. Delete the source-file grep test and add a behavioral cue listener that asserts target ASC, location, source/effect causer, and exactly-once dispatch.
4. Split authored and implicit close counters; update probe/result JSON and assertions.
5. Add `Super::ActivateAbility` and the explicit fail-closed activation branch, preserving cleanup idempotence.
6. Run focused/full regressions, archive, hand off, apply findings, and rerun.

### Named tests and commands

- `Aura.Migration.Crunch.TagRegistration`
- `Aura.Migration.Crunch.ImpactCueDispatchBehavior`
- `Aura.Migration.Crunch.AuthoredCloseAccounting`
- `Aura.Migration.Crunch.ActivationPermissionFailureCleansUp`
- Existing `Aura.Migration.CrunchCombo.RuntimePlayback`
- Existing `Aura.Abilities.FireBolt`
- Full `Aura.RoleBattle` discovery with zero failures; do not hardcode the old 237 count.

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario ReviewRemediation -RunId crunch-d02-review
& C:/Git/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/Git/AuraProj/Aura.uproject -ExecCmds="Automation RunTests Aura.Migration.Crunch; Automation RunTests Aura.Abilities.FireBolt; Automation RunTests Aura.RoleBattle; Quit" -unattended -nop4 -nullrhi
```

### Fixtures, failure semantics, artifacts, and gate

The fixture contains a hostile target ASC cue listener plus friendly/dead/outside-radius actors. Fail on zero/multiple cue dispatches, cue on a rejected target, authored-close count inflated by teardown, active task/timer after cancellation, or any FireBolt/RoleBattle regression. Store automation JSON/logs, counter schema, diff check, archive, and resolved review packet. Day 02 passes only after F1, F2, F5, and the two latent activation issues are either fixed or independently disproved with executable evidence.

### Defer / anti-goals

Do not change combo timing, role grants, imported content, damage tuning, or listen fallback policy on this day.

## Day 03 — Unify authored combo timing and deterministic network boundaries

### Player outcome

Standalone, listen host, and remote client see the same authored combo rhythm. Inputs received inside the authoritative window advance; inputs received at or after close reject consistently.

### Exact change surfaces

- `AuraMeleeAttack.cpp/.h`.
- `AuraPlayerController.cpp/.h` development probe.
- `RunCrunchComboNetworkSmoke.ps1` and `RunCrunchMigration.ps1`.
- `AuraCrunchComboRuntimeTests.cpp`.
- Planned report schema `Docs/Plans/Crunch-Migration/crunch-network-result.schema.json`.

### Data and authority contract

Set fallback active only for `NM_DedicatedServer`. Listen authority consumes authored Open/Damage/Close montage events and reports `EventSource=Authored`, `FallbackActive=0`. The development probe may make its fixture mesh visible/tickable, but may not synthesize gameplay events or enable the production fallback. Replace ambiguous NearClose acceptance with two scenarios: `BeforeClose` sends early enough that the server must receive before the frozen close timestamp; `AtOrAfterClose` sends at/after that timestamp and must reject. Logs include client send time, server receive time, authoritative open/close interval, section, prediction key, and decision.

### Numbered implementation steps

1. Narrow fallback selection to dedicated server and expose an enum-like event-source diagnostic.
2. Make the listen fixture process real authored notifies without manual event dispatch.
3. Replace `NearClose` with deterministic `BeforeClose` and `AtOrAfterClose` scenarios and strict assertions.
4. Add duplicate/late packet, cancellation during each section, loss/variance, and immediate-reactivation cases.
5. Assert four authored opens/damages/closes, accepted mask `0xF`, one damage application per section, and zero pending state after teardown.
6. Review and remediate before asset migration.

### Named tests and commands

- `Aura.Migration.Crunch.Combo.ListenUsesAuthoredEvents`
- `Aura.Migration.Crunch.Combo.BeforeCloseAccepted`
- `Aura.Migration.Crunch.Combo.AtOrAfterCloseRejected`
- `Aura.Migration.Crunch.Combo.DuplicatePressExactlyOnce`
- `Aura.Migration.Crunch.Combo.CancelEverySection`
- `Aura.Migration.Crunch.Combo.ReactivateAfterCancel`

```powershell
./RunCrunchMigration.ps1 -Stage Network -Scenario ComboFull -RunId crunch-d03-full -NetworkLagMs 75 -NetworkLagVarianceMs 10
./RunCrunchMigration.ps1 -Stage Network -Scenario ComboCancel -RunId crunch-d03-cancel -NetworkLagMs 75 -NetworkLagVarianceMs 10
./RunCrunchMigration.ps1 -Stage Network -Scenario ComboBeforeClose -RunId crunch-d03-before -NetworkLagMs 75 -NetworkLagVarianceMs 10
./RunCrunchMigration.ps1 -Stage Network -Scenario ComboAtOrAfterClose -RunId crunch-d03-after -NetworkLagMs 75 -NetworkLagVarianceMs 10
```

### Fixtures, failure semantics, artifacts, and gate

Use one Development Game listen host and one real remote client at zero lag and 75 ± 10 ms one-way emulation. A decision outside the logged interval, fallback use on listen, synthetic event, mismatched section, duplicate damage, crash signature, timeout, or orphan fails. Produce per-scenario JSON/logs and an event timeline CSV. Day 03 passes only when all scenarios are deterministic in three consecutive runs per latency profile and the external review finds no unresolved authority/timing defect.

### Post-review closure contract (2026-09-02)

The independent handoff review found no architectural rollback, but it correctly keeps Day 03 provisional until two runtime lanes are evidenced: (a) a matching real dedicated server plus external client proving `EventSource=DedicatedFallback`, authoritative exactly-once damage/cue, client convergence, and timer/task cleanup; and (b) one listen run with normal movement replication enabled and a remote attacker outside the host render path. Lane (a) is now evidenced by a source-built `AuraServer`, a cooked WindowsServer stage, and a real external client: `RemoteActivationObserved=1`, server `Open=4/Damage=4/Close=4/Accepted=4/Mask=0xF/Cleanup=1`, client `COMPLETE Open=4/Close=4/Presses=3/Cleanup=1`, and no crash signature. The dedicated fallback now suppresses normal montage completion callbacks until its final-close replication grace expires. The movement-preserving listen probe is wired, but its full-combo attempt still does not reach the four-section terminal matrix; that failure remains an explicit Day 03 blocker, not a pass. `Scripts/Tests/validate_crunch_network_timeline.py` now enforces cross-event ordering/count/terminal invariants on the passing listen artifacts.

### Defer / anti-goals

Dedicated fallback is validated only on the source-built staged server; launcher/distribution parity is not claimed. Do not import final assets or tune damage.

### Remaining-day execution status (2026-09-02)

The source checkout contains all 20 hashed anchor assets, and the six required editor-owned exports are now present. A source-built `CrunchEditor` target with unavailable online/source-control plugins disabled produced `dependencyClosure`, `blueprintDefaults`, `montageSectionTimes`, `montageNotifyTimes`, `gameplayCueReferences`, and `sourceClassReferenceScan`; `RunCrunchMigrationExportPreflight.ps1` reports `READY` and `RunCrunchMigration.ps1 -Stage Fast` scenarios pass their contract gate. The 102-package hashed dependency closure is copied under target-owned `/Game/ParagonCrunch/...` paths, with no source project references.

Day 04 presentation materialization is now under validation: target-owned Crunch mesh/skeleton, four source sequence copies, an authored four-section montage, and `ABP_Crunch_AuraV4` with a `DefaultSlot` graph have been created. The target montage validator reports four sections, twelve server-triggered Aura notifies, strict per-section timing, and zero embedded source notifies. The source-built staged dedicated ComboFull probe also passes, while full Day 04 closure still requires the dedicated geometry/remote-proxy/locomotion fixture and matching packaged in-world proof; those are not being inferred from the contract-only checks.

The first source-editor probe exited before commandlet execution because `PlasticSourceControl` was unavailable and no source target receipt existed. That blocker was recovered without changing the read-only checkout: a source-built `CrunchEditor` receipt was produced with the unavailable plugins disabled, the commandlet completed, and the export preflight is now `READY`. The dedicated binary blocker was also recovered with a source-built `AuraServer` and cooked WindowsServer stage; the final ComboFull probe passes. The movement-preserving listen lane remains blocked by its incomplete four-section client/server matrix.

### Execution update (2026-09-03)

Days 05–08 have now been implemented locally as a bounded native slice. `UAuraCrunchAbilityBase` owns target revalidation, per-event/per-target dedupe, authority-only damage/launch/push, and timer/task cleanup. Native `AuraCrunchUppercut`, `AuraCrunchDash`, `AuraCrunchGroundBlast`, and `AuraCrunchTornado` classes are granted on InputTag.1–4; Crunch retains the native LMB combo, and `AbilityInfo.json` contains all five skill identities. A lazy FName-based input resolver is used by role-grant/spec paths because Unreal can construct ability CDOs before native tags register.

The focused evidence now includes AuraEditor/AuraServer Development builds, `Aura.Migration.Crunch` automation with seven discovered tests and zero failures, `Aura.RoleBattle.Day6.CrunchPersistentASCLifecycle` with five role-owned handles across three pawn replacements, and Fast preflight passes for Uppercut, Dash, AreaSkills, GroundBlast, and Tornado. The stale native combo asset assertion was updated to the target-owned `AM_CrunchComboV4` montage.

The migration is not yet complete. Day 09 still requires a freshly cooked matching package, normal Login → Loading → StartupMap host/client flow, and in-world valid/invalid-target evidence for all five skills. The current listen runner and controller probe cover the combo only; no per-skill remote network matrix or reconnect/death/respawn skill evidence exists yet. Existing package artifacts predate the four new native skills, so they cannot be used as matching completion evidence. Day 10 remains gated on those artifacts and the independent handoff review.

### Package execution update (2026-09-03)

The first package retry exposed an AutomationTool target-selection quirk: `-client` with this Game-only project leaves `GameTarget` empty, while an explicit `-target=Aura` package invocation succeeds. The explicit Win64 Development `BuildCookRun` cooked 2,371 packages for Windows and WindowsServer, staged and IoStore-packed both lanes, archived under `Saved/StagedBuilds/CrunchMigration-20260903-game`, and included `AM_UpperCut`, `AM_Dash`, `AM_GroundBlast_Targetting`, `AM_GroundBlast_Casting`, and `AM_Tornado` in both UFS manifests. The packaged Game executable mounted its IoStore content and reached `/Game/Maps/StartupMap` without fatal/assert/ensure signatures.

The packaged topology runner remains blocked because the archive contains no `AuraServer.exe`; the installed engine distribution cannot emit the matching staged server binary. This is an explicit environment/topology blocker, not a pass inferred from the successful cook. No per-skill packaged network, reconnect, death/respawn, or normal Login→Loading host/client evidence is claimed. Day 09 and the independent Day 10 handoff gate remain open.

## Day 04 — Migrate Crunch avatar, geometry, locomotion, and authored montage

### Player outcome

Selecting Crunch visibly spawns Crunch—not Aura—with correct scale, ground contact, camera/collision fit, locomotion, hit reaction, death, and the original four-hit animation cadence.

### Exact change surfaces

- Source roots from the Day 01 dependency manifest, anchored by `ParagonCrunch/.../Meshes/Crunch`, `Crunch_Skeleton`, four `Ability_Combo_0X` sequences, `Characters/Crunch/Animation/AnimBP_Crunch`, hit/death montages, required materials/textures, and referenced sounds only.
- New target assets under `Content/Assets/Characters/Crunch/{Meshes,Materials,Textures,Animations,Audio}`.
- New `Content/Blueprints/Character/Crunch/ABP_Crunch_Aura.uasset`.
- New final `Content/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchCombo.uasset`.
- `RoleInfo.h`, `AuraAbilitySystemLibrary.cpp`, `AuraCharacterBase.cpp/.h`, and `Content/Config/RoleConfig.json` for validated role geometry/presentation values.
- `Source/AuraEditor/Private/Commands` and `AuraBaselineAssetTests.cpp` for migration/validation commandlets.

### Data and authority contract

Extend role schema version only if required fields cannot be expressed safely today: capsule radius/half-height, mesh relative transform, and base walk speed are finite bounded trusted config values. Authority applies collision/movement; each client applies the same authorized presentation values on role replication. Role application snapshots and restores mesh, ABP, capsule, movement, sockets, and weapon attachment atomically on failure. The target montage uses the Crunch skeleton and a deterministic Aura-authored section/event timing contract while the source editor's protected composite-section offsets remain unavailable; those timings are explicitly provisional and cannot close the migration until source-authored offsets are exported or independently validated.

### Numbered implementation steps

1. Migrate only the hashed dependency closure into target-owned paths; resave in UE 5.5 and reject redirectors, source-project class references, or out-of-scope skin/audio bulk.
2. Build `ABP_Crunch_Aura` on Aura's animation-instance contract using Crunch locomotion assets; retain role/debuff/death signals used by AuraProj.
3. Author `AM_CrunchComboV4` from the four source sequences and frozen Open/Damage/Close timing contract; keep the current deterministic fractions marked provisional until protected source section offsets can be exported, then validate section order and no embedded duplicate notifies.
4. Calibrate capsule, mesh transform, walk speed, camera obstruction, foot contact, combat sockets, and impact/death presentation in a dedicated fixture.
5. Update Crunch RoleConfig presentation paths and native ability montage; remove prototype RuntimeV2 from production references but keep it only if a test explicitly needs a legacy fixture.
6. Test transactional rollback and client role replication; archive visual before/after evidence and complete review.

### Named tests and commands

- `Aura.Migration.Crunch.Assets.DependencyClosure`
- `Aura.Migration.Crunch.Assets.NoSourceProjectReferences`
- `Aura.Migration.Crunch.Presentation.RoleGeometryTransaction`
- `Aura.Migration.Crunch.Presentation.RemoteProxyMatchesAuthority`
- `Aura.Migration.Crunch.Presentation.LocomotionHitDeath`
- `Aura.Migration.Crunch.Combo.AuthoredMontageContract`

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario Assets -RunId crunch-d04-assets
& C:/Git/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/Git/AuraProj/Aura.uproject -run=AuraValidateCrunchAssets -unattended -nop4 -nullrhi
```

### Fixtures, failure semantics, artifacts, and gate

Use a flat floor, stairs/slope, narrow doorway, hostile/friendly targets, camera orbit, hit reaction, death/respawn, server pawn, owning client, and remote proxy. Missing dependency, shader/material fallback, retarget warning, foot penetration above the recorded tolerance, capsule clipping, socket fallback to actor origin, client/server presentation mismatch, or rollback residue fails. Store dependency manifest, redirector report, montage timing JSON, geometry measurements, and screenshots/clips. Day 04 passes only when final assets replace every temporary Aura presentation reference and the four sections play visibly on Crunch.

### Defer / anti-goals

No alternate skins, full Paragon audio library, source UI/lobby, original C++ character class, or unrelated Phase assets.

## Day 05 — Add the shared Crunch ability boundary and UpperCut

### Player outcome

Num1 launches Crunch and valid nearby enemies into the authored airborne chain; follow-up LMB presses advance only during valid windows, while friendly/dead/out-of-range targets remain unaffected.

### Exact change surfaces

- New `Source/Aura/Public|Private/AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.*`.
- New `.../AuraCrunchUppercut.*`.
- New `Content/Blueprints/AbilitySystem/Crunch/Abilities/GA_CrunchUppercut.uasset` and translated cost/cooldown effects.
- Imported UpperCut montage and minimal dependencies under `Content/Assets/Characters/Crunch/Animations/Abilities`.
- Tags in `AuraGameplayTags.h/.cpp` and `DefaultGameplayTags.ini`; metadata in `AbilityInfo.json`.
- New `Source/Aura/Private/Tests/AuraCrunchUppercutTests.cpp`.

### Data and authority contract

The shared base owns activation identity, server target revalidation, relationship/life/range filtering, per-activation hit dedupe, typed rejection, and idempotent teardown. UpperCut translates the frozen source launch speed, hold speed, section map, damage/push curves, cost, and cooldown. Server movement/launch is authoritative; owning client may predict only its montage/self motion and reconciles to server movement. LMB follow-up during UpperCut is routed to the active UpperCut activation and cannot also activate the ground combo.

### Numbered implementation steps

1. Implement the smallest shared Crunch activation/target/cleanup boundary; do not duplicate Aura's damage resolver.
2. Add UpperCut montage tasks, launch commit event, airborne section windows, and explicit combo/LMB input arbitration.
3. Revalidate every target at each damage event and apply translated Aura damage params once per target/section.
4. Cancel on death/stun/role teardown/disconnect and restore movement/gravity state.
5. Add metadata/config as non-LMB `InputTag.1`, then validate local, listen, cancellation, and regression lanes.
6. Archive, independently review, apply findings, and rerun.

### Named tests and commands

- `Aura.Migration.Crunch.Uppercut.CommitAndLaunch`
- `Aura.Migration.Crunch.Uppercut.FollowupWindowArbitration`
- `Aura.Migration.Crunch.Uppercut.TargetMatrix`
- `Aura.Migration.Crunch.Uppercut.DuplicateEventExactlyOnce`
- `Aura.Migration.Crunch.Uppercut.CancelRestoresMovement`
- `Aura.Migration.Crunch.Uppercut.RemoteConvergence`

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario Uppercut -RunId crunch-d05-fast
./RunCrunchMigration.ps1 -Stage Network -Scenario Uppercut -RunId crunch-d05-listen -NetworkLagMs 75 -NetworkLagVarianceMs 10
```

### Fixtures, failure semantics, artifacts, and gate

Fixture: ground/air Crunch, hostile/friendly/dead/outside targets, ledge/ceiling, cancellation at launch and every follow-up, duplicate/late input, pawn destruction. Fail on client-owned damage, double launch, ceiling penetration without bounded recovery, gravity/movement residue, concurrent ground combo, or leaked task. Store target/damage ledgers, movement traces, logs, and host/client clips. Day 05 passes only when the full source-defined chain converges and existing combo/FireBolt/FireGun behavior remains green.

### Defer / anti-goals

No global launch/stun rewrite, generic airborne-combat framework, or source GameplayEffect class import.

## Day 06 — Implement Dash

### Player outcome

Num2 performs Crunch's forward dash with responsive local motion, bounded collision, one contact result per hostile target, and clean interruption.

### Exact change surfaces

- New `AuraCrunchDash.cpp/.h` and `GA_CrunchDash.uasset`.
- Imported `AM_Dash` and minimal cue dependencies.
- Shared Crunch base, gameplay tags, AbilityInfo, tests, and network runner scenarios.
- Character movement integration only where the existing movement component cannot express the frozen dash contract.

### Data and authority contract

Direction is captured from server-validated control/avatar orientation at activation. Client prediction is cosmetic/movement prediction; server owns dash distance/duration, collision sweep, cooldown/cost, hit membership, damage, push, and final transform. A target can be hit once per activation. World collision stops or slides according to the Day 01 manifest; dash cannot tunnel, cross invalid nav/world blockers, persist after cancel, or leave a speed/effect modifier.

### Numbered implementation steps

1. Translate frozen dash montage, start event, duration, range, hit radius, push, cost, cooldown, and cue.
2. Implement bounded movement using CharacterMovement-compatible prediction/reconciliation; avoid indefinite next-tick input loops.
3. Sweep server collision and revalidate targets through the shared boundary.
4. Add interruption and teardown for wall hit, stun, death, disconnect, role change, and montage end.
5. Prove correction under lag/loss and no repeated target damage during overlap.
6. Archive/review/remediate before Day 07.

### Named tests and commands

- `Aura.Migration.Crunch.Dash.DistanceAndDuration`
- `Aura.Migration.Crunch.Dash.WorldCollisionNoTunnel`
- `Aura.Migration.Crunch.Dash.TargetExactlyOnce`
- `Aura.Migration.Crunch.Dash.InvalidDirectionRejected`
- `Aura.Migration.Crunch.Dash.CancelClearsMovementEffect`
- `Aura.Migration.Crunch.Dash.RemoteCorrectionBounded`

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario Dash -RunId crunch-d06-fast
./RunCrunchMigration.ps1 -Stage Network -Scenario Dash -RunId crunch-d06-listen -NetworkLagMs 75 -NetworkLagVarianceMs 10
```

### Fixtures, failure semantics, artifacts, and gate

Fixture includes open floor, wall, 30/60 Hz clients, three overlapping hostiles, friendly/dead target, ledge, mid-dash cancel/death, and simulated lag/loss. Fail on distance outside the manifest tolerance, correction above the recorded bound, wall penetration, duplicate hit, lingering effect/timer, or proxy montage mismatch. Store transform CSV, hit ledger, corrections, and video. Day 06 passes after three consecutive zero/lag runs with no orphan state and independent review closure.

### Defer / anti-goals

No general dodge system, invulnerability frames unless explicitly present in the source manifest, navmesh teleport, or root-motion framework rewrite.

## Day 07 — Implement GroundBlast and Tornado

### Player outcome

Num3 previews and commits a legal GroundBlast location; Num4 spins for the frozen duration and damages/pushes only targets reached by authored hit events. Both expose readable cues and safe cancellation.

### Exact change surfaces

- New `AuraCrunchGroundBlast.*`, `AuraCrunchTornado.*`, and a minimal server-revalidated ground-target helper under `AbilitySystem/Targeting` if no existing helper fits.
- New `GA_CrunchGroundBlast.uasset`, `GA_CrunchTornado.uasset`, translated cost/cooldown effects, montages, and minimal cues.
- Tags, AbilityInfo metadata, tests, and runner scenarios.

### Data and authority contract

GroundBlast client preview is local only. Server retraces from the authoritative pawn/camera contract, clamps to frozen range, rejects obstructed/non-walkable/out-of-world requests, recomputes target membership inside the frozen radius, then atomically commits cost/cooldown, damage/push, and cue. Tornado lasts the frozen bounded duration; each authored hit event gets a section/event sequence, and the server hit set prevents duplicate target damage beyond source-defined cadence. Cancellation ends cues, movement modifiers, and tasks immediately.

### Numbered implementation steps

1. Implement GroundBlast preview, confirm/cancel, serialized request, server retrace, range/LOS/surface validation, and radial target query.
2. Translate cast/target montages, push/damage, camera/impact cue, and commit order; never index absent target data.
3. Implement Tornado montage, duration deadline, authored hit-event routing, per-event dedupe/cadence, push, cancel, and timeout.
4. Cover forged target data, target movement between preview/commit, duplicate hit events, late confirm, death/disconnect, and retry.
5. Run combined concurrency cases proving the role cannot activate blocked skills simultaneously.
6. Archive/review/remediate.

### Named tests and commands

- `Aura.Migration.Crunch.GroundBlast.ServerRetraceAndClamp`
- `Aura.Migration.Crunch.GroundBlast.CancelConsumesNothing`
- `Aura.Migration.Crunch.GroundBlast.TargetMatrixAndCue`
- `Aura.Migration.Crunch.GroundBlast.MalformedTargetDataFailsClosed`
- `Aura.Migration.Crunch.Tornado.DurationAndCadence`
- `Aura.Migration.Crunch.Tornado.DuplicateEventDeduped`
- `Aura.Migration.Crunch.Tornado.CancelAndTimeoutCleanup`
- `Aura.Migration.Crunch.SkillBlockingMatrix`

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario AreaSkills -RunId crunch-d07-fast
./RunCrunchMigration.ps1 -Stage Network -Scenario GroundBlast -RunId crunch-d07-ground -NetworkLagMs 75 -NetworkLagVarianceMs 10
./RunCrunchMigration.ps1 -Stage Network -Scenario Tornado -RunId crunch-d07-tornado -NetworkLagMs 75 -NetworkLagVarianceMs 10
```

### Fixtures, failure semantics, artifacts, and gate

Fixture includes valid floor, wall/ceiling/void/over-range locations, moving hostile, friendly/dead actors, clustered targets, duplicate events, cancellation at every phase, and disconnect. Fail on trust of client membership/location, cost on canceled/invalid target, cue without commit, out-of-range damage, duplicate cadence, duration overrun beyond tolerance, or cleanup residue. Store trace decisions, target ledgers, cue receipts, duration timeline, logs, and clips. Day 07 passes only when both skills meet the frozen source semantics and cross-skill blocking is deterministic.

### Defer / anti-goals

No general targeting framework replacement, destructible terrain, persistent tornado actor, or crowd-control taxonomy beyond translated source behavior.

## Day 08 — Integrate the five-skill role, HUD, persistence, and lifecycle

### Player outcome

Crunch enters with five correct controls and visible skill icons/cooldowns. Aura and BungeeMan remain unchanged. Death, respawn, pawn replacement, reconnect, and role reload do not duplicate or lose Crunch skills.

### Exact change surfaces

- `Content/Config/RoleConfig.json` and `AbilityInfo.json`.
- `AuraAssetManager.cpp`, `AuraAbilitySystemComponent.cpp/.h`, `AuraPlayerState.cpp/.h`, and role validation/application code.
- WebUI/HUD ability payload and existing skill panel assets only where new metadata is not already displayed generically.
- `AuraRoleBattleTests.cpp`, new `AuraCrunchRoleIntegrationTests.cpp`, and packaged config validators.

### Data and authority contract

Crunch RoleConfig owns exactly five input selectors: LMB plus startup abilities 1–4. All five are role-ledger handles with unique ability and input tags. Reapply is idempotent; authorized role change removes the old role set before publishing the new set; rollback restores the prior complete ledger/presentation. Save data may persist role and level, not active ability instances, targeting previews, montage sections, hit sets, timers, or in-flight cooldown rewrites. UI reads replicated authoritative specs/cooldowns and sends only existing narrow input commands.

### Numbered implementation steps

1. Add four startup ability classes and complete AbilityInfo metadata/icons/backgrounds/descriptions.
2. Extend role validation to reject duplicate inputs/tags, missing assets, invalid Crunch sockets/geometry, mixed native/XML selectors, and underconfigured CDOs.
3. Prove grant/revoke/rollback across fresh join, three pawn replacements, death/respawn, reconnect, late join, and save load.
4. Publish five HUD slots and cooldown/targeting/cancel feedback without role-specific authority in JavaScript.
5. Run Aura, BungeeMan, Crunch, and Civilian regression matrices.
6. Archive/review/remediate before packaging.

### Named tests and commands

- `Aura.Migration.Crunch.Role.ExactFiveInputOwners`
- `Aura.Migration.Crunch.Role.ThreePawnReplacementIdempotence`
- `Aura.Migration.Crunch.Role.ReconnectAndLateJoin`
- `Aura.Migration.Crunch.Role.RollbackRestoresPriorRole`
- `Aura.Migration.Crunch.Role.NoInFlightStatePersistence`
- `Aura.Migration.Crunch.HUD.FiveSkillMetadata`
- Existing `Aura.Abilities.FireBolt`, FireGun, WebUI content/contract, and full `Aura.RoleBattle` suites.

```powershell
./RunCrunchMigration.ps1 -Stage Fast -Scenario RoleIntegration -RunId crunch-d08-role
```

### Fixtures, failure semantics, artifacts, and gate

Use one persistent PlayerState ASC, three pawn replacements, all selectable roles, save/reconnect, late join during each active skill, malformed configs, and actual HUD HTML. Fail on duplicate/missing spec, input collision, stale cooldown/targeting UI, persisted transient state, wrong mesh/ABP, fallback to another role, or regression. Store grant-ledger snapshots, config hashes, HUD payloads/screenshots, lifecycle logs, and test results. Day 08 passes only with exactly five Crunch owners through all lifecycle cases and unchanged Aura/BungeeMan contracts.

### Defer / anti-goals

No role hot-swap during live combat unless already supported, skill-tree progression, arbitrary remapping UI, or WebUI gameplay authority.

## Day 09 — Matching packaged normal-flow matrix

### Player outcome

A user launches the packaged game normally, selects Crunch, connects to the matching packaged server, spawns visibly as Crunch, moves, and uses all five skills with correct remote presentation and authoritative results.

### Exact change surfaces

- `RunCrunchMigration.ps1` package/network/final stages.
- Game Server Manager/startup scripts only if the matching-build launch path is defective.
- Package manifest validation for maps, config, Crunch assets, ability classes/effects, cues, WebUI, and shader/material dependencies.
- Evidence only under `Saved/Reports/CrunchMigration/...`; no source changes solely to fake observability.

### Data and authority contract

Host and client must use the same target revision, package hash, RoleConfig/AbilityInfo hashes, network version, and source-asset manifest hash. The normal journey is `Aura.exe → Login → select Crunch → Connect → Loading → StartupMap → controllable Crunch`. The server is launched from that exact package; no older `192.168.1.6:7790`, editor server, or different archive may satisfy the gate.

### Numbered implementation steps

1. Clean-build/cook/stage/archive Win64 Development Game and verify the complete package manifest.
2. Launch the packaged listen host and one packaged remote client through the normal manager/Login path with matching hashes.
3. Capture host and remote views of spawn, locomotion, LMB Combo, UpperCut, Dash, GroundBlast, Tornado, invalid/friendly rejection, cancellation, death/respawn, and reconnect.
4. Compare server target/damage/commit ledgers with visible outcomes and HUD cooldowns.
5. Repeat at zero lag and 75 ± 10 ms one-way emulation; run a 15-minute mixed-skill loop with bounded memory/task counts.
6. Record dedicated-server preflight separately as PASS or exact environment `BLOCKED`; never substitute listen evidence.

### Named tests and commands

- `Crunch.Package.ManifestComplete`
- `Crunch.Package.HashHandshakeMatches`
- `Crunch.Package.NormalFlowAllSkills`
- `Crunch.Package.RemotePresentationAndAuthority`
- `Crunch.Package.DeathReconnectCleanup`
- `Crunch.Package.MixedSkillSoak15m`

```powershell
./RunCrunchMigration.ps1 -Stage Package -Scenario Build -RunId crunch-d09-package
./RunCrunchMigration.ps1 -Stage Package -Scenario NormalFlow -RunId crunch-d09-normal
./RunCrunchMigration.ps1 -Stage Network -Scenario AllSkills -RunId crunch-d09-lag -PackageReportPath <explicit-package-report> -NetworkLagMs 75 -NetworkLagVarianceMs 10
```

### Fixtures, failure semantics, artifacts, and gate

Use one packaged listen host, one packaged remote client, hostile/friendly/dead/outside targets, obstacles, valid ground pads, death/respawn, reconnect, and actual HUD. `OutdatedClient`, Login/Loading-only progress, firewall/security prompt, hash mismatch, missing asset/cue, invisible skill, server/client outcome mismatch, crash, timeout, leak, or orphan fails and leaves the day open. Store package hash/manifest, process map, screenshots/clips for every skill on both views, logs, ledgers, soak metrics, and dedicated preflight. Day 09 passes only with in-world matching-build visual and behavioral proof; screenshots of Login do not count.

### Defer / anti-goals

No production identity/account claim, public internet deployment, dedicated PASS without a capable toolchain, or manual firewall/security-policy automation.

## Day 10 — Independent review and honest sign-off

### Player outcome

Crunch is a complete, supportable AuraProj role for the declared local/LAN listen scope, with explicit limitations rather than hidden evidence gaps.

### Exact change surfaces

- All Crunch-owned source/config/content/tests/runner changes from Days 01–09.
- New `Docs/Reports/Crunch-Migration-Final-Signoff-2026-09-XX.md`.
- New immutable final result `Saved/Reports/CrunchMigration/<Revision>/<RunId>/final.json` plus copied evidence links/hashes in the report.
- Required final visual change archive and `.claude/memory/visual-change-archive.md` entry.

### Data and authority contract

`final.json` is produced once from explicitly named Day 01–09 artifacts whose revision/config/package/source-manifest hashes all match. It reports each required lane `PASS`, `FAIL`, or `BLOCKED` with reason; it cannot turn missing evidence into PASS. Local/LAN listen completion requires zero P0/P1 defects, zero unexplained warnings, no authority/data/lifecycle violation, and all human-visible gates. Dedicated/release remains independently classified.

### Numbered implementation steps

1. Run clean build, complete focused Crunch suite, full RoleBattle/ability/UI regressions, asset validators, all network scenarios, package matrix, soak, JSON/XML/SVG/link checks, and `git diff --check` at one revision.
2. Audit every pending-review finding and every daily independent-review response; link the fix or executable disproof.
3. Send the final private code/evidence packet through `in-app-chatgpt-handoff`; apply demonstrated findings and rerun affected evidence before signing.
4. Verify no source-project runtime references, prototype production references, stale packages, orphan processes, temporary Aura adapter, or unowned pending files remain.
5. Publish final.json, human report, final before/after/validation SVG, and searchable archive entry.
6. Declare complete only if all mandatory local/listen rows pass; otherwise publish `NOT COMPLETE` with exact reopening day and failed artifact.

### Named tests and commands

- Entire `Aura.Migration.Crunch` namespace with zero failures.
- Full discovered `Aura.RoleBattle`, Aura ability, FireGun, FireBolt, WebUI, asset, JSON, package, and runner suites.
- `Crunch.Final.ArtifactConsistency`
- `Crunch.Final.NoSourceOrPrototypeReferences`
- `Crunch.Final.RequiredVisualEvidence`
- `Crunch.Final.ReviewFindingsResolved`

```powershell
./RunCrunchMigration.ps1 -Stage Final -Scenario Complete -RunId crunch-final-<revision> -PackageReportPath <explicit-package-report>
git diff --check
```

### Fixtures, failure semantics, artifacts, and gate

No partial aggregate is accepted. A failed/timed-out review, mismatched hash, missing visual, unverified skill, unresolved P0/P1, unexplained warning, or dirty-file ownership gap leaves final status `NOT COMPLETE`. The final gate is satisfied only when Days 01–09 are linked and green at one target revision, the final external review is resolved, normal-flow packaged host/client evidence covers all five skills, and the report distinguishes Local/LAN Listen PASS from Dedicated/Release PASS or BLOCKED.

### Defer / anti-goals

Do not include Phase, minions, shop/inventory, original lobby/session architecture, alternate Crunch skins, new character selection framework, balance certification, or production online-provider release. Those require separate plans and cannot be used to delay or inflate this bounded migration.

## Global completion gate

The Crunch Character/Pawn behavior migration is complete only when all of the following are true at one recorded target revision:

1. Crunch uses its own target-owned mesh, skeleton-compatible Aura animation blueprint, locomotion/hit/death presentation, combat sockets, collision calibration, and final authored montages; no temporary Aura mesh/ABP or production prototype montage remains.
2. LMB Combo, Num1 UpperCut, Num2 Dash, Num3 GroundBlast, and Num4 Tornado match the frozen source semantic manifest and are granted exactly once by the Crunch role.
3. Server authority, target revalidation, exactly-once effects, cost/cooldown commit, movement reconciliation, cancellation, teardown, role persistence, and HUD contracts pass focused and regression coverage.
4. Matching packaged listen host and remote client complete the normal Login-to-game journey and visibly demonstrate every skill, invalid-target rejection, death/respawn, and reconnect at zero and emulated latency.
5. All daily and final independent review findings are applied or disproved by executable evidence; final handoff limitations are stated honestly.
6. Build, asset/config validation, focused/full automation, network scenarios, package manifest/hash handshake, mixed-skill soak, diff/link/JSON/XML/SVG checks, and child-process cleanup pass.
7. Dedicated and production release status is independently `PASS` or exact `BLOCKED`; it is never inferred from listen/local proof.

If any row is absent, stale, hash-mismatched, timed out, or replaced by Login/Loading-only evidence, the migration remains **NOT COMPLETE**.
