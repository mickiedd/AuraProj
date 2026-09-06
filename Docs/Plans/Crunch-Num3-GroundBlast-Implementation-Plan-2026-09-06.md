# Crunch Num.3 GroundBlast implementation plan

Date: 2026-09-06. Status: inspected and planned; implementation milestones below are pending.

## Scope and inspected baseline

This job plans completion of the existing Num.3 ability, not a second skill or a second grant path. Inspection covers current native source, role/metadata configuration, tests, migration runner, and original Crunch source at `C:/Works/Crunch-master`. No gameplay build or live playtest was run for this documentation job. Historical reports are evidence of earlier runs, not current runtime certification.

| Feature | Current implementation and evidence | Confidence / remaining work |
|---|---|---|
| Role selection and presentation | `Content/Config/RoleConfig.json` publishes Crunch, melee combat profile, `SM_CrunchV4`, and `ABP_Crunch_AuraV5`. | Configuration verified. September 5 locomotion archive records idle/jog into DefaultSlot and earlier passing runtime checks. |
| LMB | Native `UAuraMeleeAttack`, migrated combo montage; `AuraCrunchComboRuntimeTests.cpp` and combo network scripts exist. | More developed than numbered skills; preserve combo ownership and regression tests. |
| Num.1 | `AuraCrunchUppercut.cpp`: montage, launch/damage events, timed fallbacks, follow-up input sections, bounded cleanup. | Implemented source slice; no fresh functional proof in this inspection. |
| Num.2 | `AuraCrunchDash.cpp`: montage, start event plus fallback, movement tuning, bounded movement/hit loop and restoration. | Implemented source slice; retain behavior while changing targeting input. |
| Num.3 | `AuraCrunchGroundBlast.cpp`: immediate mouse target task, server ground retrace, commit, radial query, damage/push parameters, cue and local montage play. | Incomplete interaction and presentation; main scope of this plan. |
| Num.4 | `AuraCrunchTornado.cpp`: montage/event task, bounded repeating radial hits and per-event target ledger. | Implemented source slice; no fresh functional proof. |
| Shared GAS boundary | `AuraCrunchAbilityBase.cpp`: LocalPredicted, InstancedPerExecution, server damage, combat-rule filtering, hit ledger and owned timer cleanup. | Reuse; changes to shared code require all four numbered skill regressions. |
| Grants and HUD | Crunch owns the four native startup classes plus native LMB; `AbilityInfo.json` contains GroundBlast metadata but uses FireBolt artwork. | Identity/input tests and persistent-ASC tests exist; metadata presence does not prove correct presentation. |

Key source references are relative to repository root unless an absolute source-checkout path is given:

- `Source/Aura/{Public,Private}/AbilitySystem/Abilities/Crunch/AuraCrunchGroundBlast.{h,cpp}`.
- `Source/Aura/Private/AbilitySystem/AbilityTasks/TargetDataUnderMouse.cpp`: samples once when activated; no preview/confirmation phase.
- `Source/Aura/Private/Tests/AuraCrunchUppercutTests.cpp`: identity, policy and config-string checks, not GroundBlast execution tests.
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`: role grants and persistent ASC lifecycle.
- `RunCrunchMigration.ps1`: reports **contract preflight**; Network accepts combo scenarios only. A GroundBlast Fast PASS is not a runtime or network pass.
- `C:/Works/Crunch-master/Source/Crunch/Private/GAS/GA_GroundBlast.cpp` and `TargetActor_GroundPick.cpp`: user-confirmed targeting actor, moving decal, aim state, basic-attack blocking, cancellation, blast and camera-shake cues. The original camera-forward targeting should be adapted to Aura's cursor controls, not copied blindly.

## Gaps and decisions

1. Add a real targeting lifecycle. Current targeting montage is immediately followed by cursor submission; there is no movable radius preview, explicit confirmation, cancellation input or scoped LMB ownership.
2. Preserve server authority, and correct target validation. Current validation only retraces vertically; despite its rejection log mentioning LOS, it does not test caster-to-point obstruction. Horizontal clamping also reconstructs Z from the caster before the height check, losing the requested elevation. Validate the final retraced point as well as the request.
3. Define blast membership separately from cast-center range. Current collection also filters victims by distance from the caster using 2000 units, which truncates the far side of a radius-300 blast centered at maximum range.
4. Verify damage and push through real attributes/movement. The inherited knockback chance defaults to zero; merely setting force does not demonstrate a push. Verify the damage library's fallback effect and configure deterministic GroundBlast push without changing the other skills accidentally.
5. Use GAS-owned cast presentation. Current raw server `Montage_Play` followed by immediate end does not establish remote-owner/observer montage delivery or recovery. Cue execution likewise needs a resolved cue asset and visible runtime evidence.
6. Close lifecycle races. `ReadyForActivation()` can synchronously invoke callbacks and end the ability before the current code registers its timeout. Arm deadlines before task activation, and guard every callback by active phase/activation identity. Per-execution instancing means `bCommitted` is not by itself a proven cross-cast persistence defect.
7. Preserve existing source semantics where understood: damage occurs on accepted confirmation, not at an invented later animation notify. Exported Blueprint balance/cue defaults remain a verification gate; native defaults are not proof of authored Blueprint values.

## Target behavior and ownership contract

Proposed control decision: press Num.3 to aim; move the cursor to reposition a ground circle; LMB confirms; RMB cancels. Consume the corresponding press/held/release sequence while aiming so confirmation cannot also attack or move. Releasing Num.3 does not cast. Another Num.3 press during aiming is ignored. Keep a 5-second aiming deadline initially. These are explicit Aura adaptation decisions, subject to playtest adjustment, not claims about source key bindings.

State flow: `Idle -> Aiming -> AwaitingAuthority -> Casting -> Ended`, with cancellation/rejection/timeout paths to `Ended`. Local preview is cosmetic and owned by the initiating player. Server owns accepted center, cost/cooldown, victims, damage, push and cast outcome. Simulated proxies display accepted casting/blast only. Accepted confirmation deals damage immediately; casting remains alive until montage completion or a bounded recovery watchdog. There is one commit and one hit per victim per activation.

Initial retained values: radius 300 uu, center range 2000 uu, maximum height difference 250 uu, fallback physical damage 45, requested push speed 3000 uu/s, mana/cooldown 0. Record source Blueprint overrides before finalizing values. Use a nonzero-cost/cooldown test fixture even if shipping defaults stay zero.

Client sends one typed location request tied to ability spec and prediction key, never an authoritative victim list. Reject malformed counts/types, nonfinite coordinates, stale/duplicate requests and invalid surfaces before commitment. Preview clamps horizontal center range while preserving candidate height. Server repeats clamp, ground trace, normal threshold (initially Z >= 0.5), final range/height checks and caster-to-center LOS using explicitly selected collision channels. Include trace tolerance in the frozen contract. Do not accept client-provided normal, actor or blocking flag as proof of ground.

Blast victims are server overlaps about the accepted center, deduplicated and filtered through Aura combat rules/life state. A valid cast at an empty location still commits and presents. Preserve the existing spherical area initially; document that per-victim wall occlusion is deferred, while caster-to-center LOS is mandatory. Damage membership must not be inadvertently clipped by the center's cast range. Revalidate life/relationship when damage applies.

No pre-confirmation charge. Cancellation, invalid input, missing required assets, rejection or timeout before commit consumes nothing. Post-commit interruption never refunds or repeats damage. Death, stun, avatar replacement, role change, disconnect and explicit cancellation must dispose of preview, tasks, input ownership, delegates and timers. Late callbacks cannot mutate a later activation. Missing world/controller/ASC fails closed. Do not change global cursor-task semantics for other abilities.

## Day 01 — Freeze source and integration contracts

Dependencies: none. Surfaces: existing source export scripts/manifests, this plan, `Content/Config/{RoleConfig,AbilityInfo}.json`, GroundBlast header, controller/ASC input handlers, gameplay tags, `Actor/MagicCircle` and cue discovery configuration.

1. Capture current git status and preserve unrelated BungeeMan changes and prior archive entries. Read source Blueprint defaults, target montage sections/notifies and cue references through the existing source-editor export/preflight workflow; explicitly list unavailable fields.
2. Record confirmed balance values, targeting loop/cast lengths, skeleton/slot compatibility, valid surface channels and trace tolerance in `Docs/Plans/Crunch-Migration/crunch-groundblast-contract.json` (new). Distinguish copied values from approved Aura adaptations.
3. Trace physical Num.3 and WebUI input into controller/ASC. Specify scoped confirm/cancel consumption ahead of normal LMB attack/movement and interaction with existing placement mode. Only one targeting mode may own confirmation at a time; reject entry when another mode owns it.
4. Inventory reusable MagicCircle/decal and blast/audio/shake assets; resolve actual paths and packaging dependencies rather than assuming a cue tag has a handler.

Verification: run the existing Fast GroundBlast preflight (command below); inspect exports for exact field availability. Deliver contract JSON and source/asset evidence paths under `Saved/Reports/CrunchGroundBlast/<run-id>/`. Gate: no unresolved required montage/cue/input path; unknown source values explicitly resolved or retained as documented Aura defaults. Failure: export/preflight failure blocks asset-parity claims; do not silently guess. Defer: other skills' balance and a global targeting-system rewrite.

## Day 02 — Implement aiming and input lifecycle

Dependencies: Day 01 frozen contract. Surfaces: GroundBlast `.h/.cpp`; new `Source/Aura/{Public,Private}/AbilitySystem/AbilityTasks/CrunchGroundTarget.{h,cpp}`; `Source/Aura/{Public,Private}/Player/AuraPlayerController.{h,cpp}`; ASC input methods only if necessary; existing MagicCircle API or a dedicated target actor; new GroundBlast tests.

1. Implement explicit phases and per-activation ownership. Create the local preview with valid/invalid feedback and radius from the frozen contract. Update the candidate every local frame while aiming; no ticking preview on dedicated server.
2. Register bounded deadlines/delegates before activating tasks. Integrate LMB confirm and RMB cancel in physical and WebUI routes; retain ownership through release of consumed buttons. Suppress held Num.3 reactivation until release.
3. Publish one target request through GAS prediction-key-aware target data. Register server callbacks before waiting, consume received data and unregister callbacks on task destruction. Never change `UTargetDataUnderMouse` behavior for FireBolt or other users.
4. Add centralized idempotent cleanup and aim/basic-attack blocking with target-owned tags. Prevent combo overlap according to the frozen input contract; release only state this activation owns.

Named new tests: `Aura.Migration.Crunch.GroundBlast.TargetingLifecycle`, `.ConfirmConsumesLMB`, `.CancelNoCommit`, `.HeldKeySingleActivation`, `.SynchronousCallbackCleanup`. Fixtures: local player controller plus ASC, preview actor counter, injected immediate callback and missing-controller case. Assertions: no damage while aiming; exactly one confirm; no combo/movement click leakage; no preview/timer/delegate after cancel or 5-second timeout; next activation works. Gate: tests and visible preview/confirm/cancel pass. Failure: task creation/required preview failure cancels without charge. Defer: gamepad targeting and new HUD architecture.

## Day 03 — Complete authoritative targeting, damage and push

Dependencies: Day 02 lifecycle. Surfaces: GroundBlast validation/commit; shared Crunch base only for a narrowly scoped center-vs-victim-range API; new `Source/Aura/Private/Tests/AuraCrunchGroundBlastTests.cpp`; existing combat-rule fixtures.

1. Validate request schema and activation identity, retrace/clamp and revalidate the final point. Add caster-to-point LOS with self ignored and documented start offset. Resolve invalid targets before `K2_CommitAbility`.
2. Separate center validity from victim membership. Query sphere at accepted point, use combat-rule filtering, and ensure multiple collision components cannot multiply damage.
3. Apply one authoritative damage effect and deterministic configured push to eligible targets. Resolve zero-length push direction deterministically and document behavior for non-character targets. Verify fallback damage-effect resolution; do not infer success from calling a helper.
4. Make commit atomic at the ability phase boundary. Duplicate requests and cancellation racing with confirmation can produce at most one accepted commit. Revalidate current avatar/life before accepting.

Named tests: `.TargetValidation`, `.FinalPointBounds`, `.BlockedLOS`, `.BlastEdgeMembership`, `.AuthorityDamageAndPush`, `.DuplicateAndStaleData`, `.CostCommitBoundary`. Fixture topology: flat floor, steep ramp, elevated ledge, wall, void; self, ally, live enemy, dead enemy, multi-component enemy, outside-radius enemy, enemy beyond 2000 from caster but within valid blast radius. Test NaN/infinity, extreme finite values, wrong payload type/count, final retrace outside height bound, repeated activation and empty valid cast. Use observed health delta, commit count, GE application count and movement velocity; raw configured 45 is not always final damage after mitigation. Gate: server-only mutation, no rejected-request charge, boundary victims behave as specified. Failure: invalid data terminates cleanly; no retry charge. Defer: per-victim occlusion and cross-skill damage redesign.

## Day 04 — Replicated casting, cues and recovery

Dependencies: Day 03 accepted-result contract. Surfaces: GroundBlast montage task; target-owned `Content/Assets/Characters/Crunch/Animations/Abilities/AM_GroundBlast_{Targetting,Casting}.uasset` as needed; resolved cue assets; `Content/Config/AbilityInfo.json`; relevant AuraEditor presentation/asset tests.

1. Transition from targeting to casting without the targeting task's interruption callback cancelling the accepted cast. Use ASC/GAS montage ownership so owner and observers receive casting state; handle rejection of predicted aiming.
2. Emit the accepted blast once with server center/radius. Resolve a target-owned blast handler and optional source-equivalent local camera shake; ensure shake is scoped to the intended player rather than all controllers.
3. Retain immediate-on-confirm damage semantics. End casting on completion/interruption; watchdog equals verified montage duration/rate plus 0.5 seconds, with a documented hard upper bound. Missing required cast assets reject before commit; unexpected post-commit presentation failure cleans up without refund/re-hit.
4. Replace borrowed FireBolt artwork with an existing suitable GroundBlast asset if available; otherwise explicitly record the remaining art placeholder. Confirm locomotion returns through V5 DefaultSlot.

Tests: `.CastPresentation`, `.CueOnce`, `.RecoveryAndAvatarReplacement` and existing `Aura.Migration.Crunch.Presentation.LocomotionGraph`, `Aura.Migration.CrunchCombo.RuntimePlayback`. Capture owning-client and observing-client aim/cast/recovery frames; any Blueprint edits require post-job visual inspection of the edited graph and runtime result. Gate: visible radius matches gameplay area, cue/cast once, clean locomotion after success and interruption. Failure: any missing rendered proof remains an explicit presentation gate. Defer: unrelated animation improvements and custom artwork generation unless needed.

## Day 05 — Runtime proof, regressions and review

Dependencies: Days 01–04. Surfaces: new `RunCrunchGroundBlastNetworkSmoke.ps1`, new GroundBlast runtime fixture following existing combo-fixture patterns, new `Scripts/Tests/validate_crunch_groundblast_timeline.py`, runner tests and dedicated result schema. Keep existing combo runner claims accurate.

1. Implement a real launch-and-observe runner: standalone, listen server with remote owner and separate observer, dedicated server with two clients. Run host-owner and remote-owner cases where applicable. Add a 100 ms lag / 20 ms variance run; exercise supported packet-loss injection separately and record actual settings.
2. Correlate role/spec, activation identity, accepted point, server damage/commit counts, client presentation and cleanup in versioned results. Never pass because a process started or a config string exists. Keep expected outcomes independent of telemetry generation.
3. Run success, invalid target, no enemies, confirm spam, cancel-before-confirm, timeout, death/stun, disconnect, avatar replacement and second successful activation. Verify observers do not own a preview and clients never apply damage.
4. Build/cook a Development Win64 client and dedicated server using existing project packaging conventions, then run the packaged fixture. Require actual logs from the package, not editor logs. Record engine version, revision plus dirty-diff fingerprint, fixture map and exact commands.
5. Run all Crunch identity/role/lifecycle/combo regressions and unaffected cursor ability input checks. Obtain independent `in-app-claude-handoff` review and apply findings before closing implementation. Existing migration authorization covers private evidence; if unavailable, record the reason and follow the documented local-validation fallback. Create a new immutable visual archive for the completed implementation.

Runner contract: bounded 120-second startup and 180-second scenario deadlines per topology; terminate only run-owned processes, preserve logs, write failure/blocked result and return nonzero on missing actor/timeout/crash/assertion. No automatic retry that masks an initial failure. Network results must include damage-on-authority, no-client-damage, presentation-on-owner/observer and cleanup assertions. Gate: all required topologies and regressions pass; package evidence and visual review exist; review findings are addressed. Defer: shipping/performance certification, unrelated dirty changes and Num.1/2/4 feature expansion.

## Commands and evidence contract

Run from `C:/Git/AuraProj`. `UE_ENGINE_ROOT` must point at the project's installed compatible engine. These are implementation validation commands, **not tests run during this planning job**.

```powershell
# Existing preflight only; not a GroundBlast execution test.
./RunCrunchMigration.ps1 -Stage Fast -Scenario GroundBlast -RunId num3-baseline-20260906
& "$env:UE_ENGINE_ROOT/Engine/Build/BatchFiles/Build.bat" AuraEditor Win64 Development "C:/Git/AuraProj/Aura.uproject" -WaitMutex
& "$env:UE_ENGINE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Git/AuraProj/Aura.uproject" -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests Aura.Migration.Crunch;Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=C:/Git/AuraProj/Saved/Reports/CrunchGroundBlast/automation'
& "$env:UE_ENGINE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Git/AuraProj/Aura.uproject" -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests Aura.RoleBattle.Day6.CrunchPersistentASCLifecycle;Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=C:/Git/AuraProj/Saved/Reports/CrunchGroundBlast/role'
# Planned new runner interface; implement in Day 05 before executing.
./RunCrunchGroundBlastNetworkSmoke.ps1 -Topology Standalone -Scenario All -RunId num3-standalone
./RunCrunchGroundBlastNetworkSmoke.ps1 -Topology Listen -Scenario All -RunId num3-listen
./RunCrunchGroundBlastNetworkSmoke.ps1 -Topology Dedicated -Scenario All -RunId num3-dedicated
./RunCrunchGroundBlastNetworkSmoke.ps1 -Topology Dedicated -Scenario All -NetworkLagMs 100 -NetworkLagVarianceMs 20 -RunId num3-lag
python Scripts/Tests/validate_crunch_groundblast_timeline.py --run-root Saved/Reports/CrunchGroundBlast/num3-dedicated
git diff --check
```

Wrap engine commands in bounded process supervision and inspect automation result counts, not just exit status; missing tests fail the gate. Rendering evidence requires an RHI-enabled run, not NullRHI. Day 05 must record the exact verified package invocation and package paths before claiming package completion.

Each milestone stores logs/results, changed-file list, contract deviations and review outcome under a unique run directory. Final completion requires targeted functional assertions, multiplayer evidence, packaged execution, visual capture, review/fallback record and implementation archive. This plan itself changes documentation only.
