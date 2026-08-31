# Day 42 — Mission Vertical Slice

Status: Planned; not implemented by this planning job.  
Depends on: Day 41 Frozen scope and runtimeEntry READY with inherited Day 40 prerequisites verified.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Play one objective-only Assault from hub to completion/failure and back, with temporary state safely isolated from the player's permanent profile.

## Exact change surfaces

- Existing: `Source/Aura/Private/Game/AuraGameModeBase.cpp`, `Public/Player/AuraPlayerState.h`, `Private/Player/AuraPlayerController.cpp::PawnLeavingGame`, `Private/Character/AuraCharacter.cpp::PossessedBy/LoadProgress`, `Public/Game/AuraPersistenceSubsystem.h`, `AuraSaveTypes.h` and matching save-record implementations.
- New: `Source/Aura/Public/Gameplay/AuraMissionTypes.h`, `AuraMissionSubsystem.h`, `AuraMissionState.h`, `AuraRunLoadoutComponent.h`, `AuraGameplayDefinitionRegistry.h` and matching private implementations.
- New: `Content/Config/GameplayMissionDefinitions.json`, `GameplayRoleLoadouts.json`, `Scripts/RunGameplayExpansionDay.ps1`, gameplay network probe; modify existing native HUD/WebUI panels only for minimal mission start/status/result.

## Data and authority contract

Implement the architecture lifecycle and durable run-entry marker before normalizing health/mana/ammo/ability level. Require at least 100 gold of unused wallet capacity for every participant before entry, even though payout arrives on Day 53; 99 rejects and 100 permits preparation. During an active run, serialization uses the committed hub snapshot. Disable gameplay-profile legacy XP/loot/civilian-kill rewards and hub commerce. Day 42's end-to-end objective is a registered three-second relay interaction and zero permanent payout; enemy-driven Clear replaces it on Day 43. Label this internal slice, never claim final Assault behavior. Use only surveyed anchors.

Before Day 51 adds rescue, accepted mission death marks the member incapacitated and suppresses legacy respawn. Survivors may continue; no live member or proxy means failure. Reconnect cannot resurrect. A disconnected participant retains a targetable dormant authority pawn/proxy, run state and profile lease for 60 seconds; existing attacks/DoTs and absolute deadlines continue. Retained state must attach before legacy LoadProgress/defaults, save, recovery normalization or MarkCombatReady can run. Expiry/abandonment restores that member's hub snapshot and clears its marker transactionally before profile release, retains a Forfeited tombstone, and cannot overwrite a later hub generation during final cleanup.

## Numbered implementation steps

1. Load and validate profile/mission/loadout definitions in world readiness. Select the profile from server launch policy only; ignore client attempts to replace it. Spawn the public mission state actor on authority and bind owner run components to stable participants.
2. Check every participant's maximum-payout wallet capacity, then introduce a shared batch prepare/commit operation over the existing persistence manifest and commit all hub snapshots and ActiveRunId markers with one manifest switch before entry. Day 53 extends this same operation with terminal receipts/payouts. Exercise logout/autosave capture to prove temporary ammo, health, specs/effects and normalized level cannot leak.
3. Implement preparation/ready/cancel and Active/Extraction/terminal transitions with RunId/epoch/revision and the 20-minute deadline. Server validates participants, roles and command ownership.
4. Add the temporary registered relay objective, 30-second extraction and success/failure UI. Technical abort returns to hub; no reward writes until Day 53.
5. Intercept accepted mission death before GameMode schedules legacy respawn, cancel pending legacy callbacks, retain incapacitation and fail when no live member/proxy remains. Keep legacy-profile death unchanged; restore hub state only after terminal cleanup.
6. Hook PawnLeavingGame and logout/profile release before engine teardown destroys retained state. Run-owned references retain the authority avatar/proxy, PlayerState/ASC and profile snapshot independently of the controller. Cancel inputs/interactions/reload on disconnect, retain completed resources and absolute cooldowns, and suspend only new admissions when no connected Alive member remains. Attach reconnect/replacement state before legacy defaults/save/readiness; attachment failure stays Dead/Recovering without an Alive/default-resource frame.
7. Commit marker cleanup and hub restoration before a member's 60-second expiry/abandonment releases its profile; retain a Forfeited tombstone. Failed commit keeps that profile locked. Terminal cleanup checks RunId/profile generation for every still-bound member, including ineligible members. Restart restores the hub and clears an abandoned marker in a new manifest generation.
8. Complete actual packaged launch/result parsing in the gameplay adapter; record public start/interact/extract inputs and authoritative outputs rather than directly setting success.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay42Tests.cpp`; namespace `Aura.Gameplay.Day42`.

- RunTransitionExactlyOnce — duplicate completion and timer events produce one terminal revision.
- RunEntryCommitFailure — failed persistence leaves the player in hub with unchanged resources.
- TransientSaveIsolation — save/logout during normalized ammo and temporary ability grant preserves the pre-run profile.
- RestartAbortsActiveRun — restart restores hub snapshot and clears the marker with no payout.
- ProfileSelectionServerOnly — remote URL/RPC options cannot override the authoritative server profile.
- RunEntryWalletCapacity — 99 gold of remaining capacity rejects before markers/loadout; 100 permits entry with the wallet unchanged.
- InterimDeathSuppressesRespawn — mission death schedules no legacy respawn; a survivor continues and no live member/proxy ends the run once.
- DormantProxyRetainsCombat — disconnect preserves targetability, committed hits/DoTs, resources and deadlines; proxy death remains dead after reconnect.
- AttachBeforeDefaultRestore — replacement/reconnect installs retained ASC/run state before LoadProgress/save/MarkCombatReady and never publishes default resources or premature Alive.
- MemberExpiryCommitBeforeRelease — failed cleanup retains the profile lock; successful expiry restores hub/clears marker before release, and later run cleanup cannot overwrite a newer hub generation.
- OldProfileStillPlayable — legacy launch retains its original reward, HUD, role and recovery behavior.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 42 -Stage Fast -RunId d42-fast
./RunGameplayExpansion.ps1 -Day 42 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d42-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

MissionLifecycle: S-A/S-B and all paired lanes; force failure before entry commit, after marker commit, after loadout grant and at extraction. Test wallet capacity 99/100, one-member and all-member deaths, and disconnect while an impact/DoT is committed. Reconnect in separate cases after 10 seconds and after 61 seconds; test dead proxy reconnect, injected attachment failure, expiry cleanup write failure, and a subsequent hub edit before the old run settles.

## Failure and timeout semantics

Preparation cancels at 30 seconds, choice-free stub scenario still uses the 20-minute run cap, and invalid definition/anchor refuses entry. Crash has no run resume. A retained live proxy remains vulnerable through the 60-second lease; all-disconnected does not freeze existing combat or deadlines. Expiry cleanup failure keeps the profile locked for repair, and failed attachment never marks Alive. A failed rollback/isolation assertion blocks all gameplay-content days. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-42.json, lifecycle-transitions.json, hub-profile-before-after.json with redacted identifiers, restart-marker-cases.json, member-lease-and-death-cases.json, attachment-order-trace.json, public HUD captures and packaged lane logs. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

A real player can start, finish/fail and restart the slice; every terminal/crash path restores the same persistent profile and no temporary state is committed. Mission death cannot trigger legacy respawn, disconnect cannot grant immunity/refill, profile release follows committed marker cleanup, and no replacement publishes default resources before retained-state attachment.

## Defer / anti-goals

No reward payout, procedural assembly, permanent progression, mid-run server restart resume, or generalized quest scripting.
