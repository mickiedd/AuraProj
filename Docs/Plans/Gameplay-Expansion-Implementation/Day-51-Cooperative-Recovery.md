# Day 51 — Cooperative Recovery

Status: Planned; not implemented by this planning job.  
Depends on: Day 50 pacing/member state; existing Day 28 player recovery and Day 42 run isolation.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Let teammates rescue each other with a clear cost, without creating infinite respawns or replacing the existing death lifecycle.

## Exact change surfaces

- Existing: `Source/Aura/Private/Game/AuraGameModeBase.cpp` player death/respawn/logout path; `Private/Character/AuraCharacter.cpp::PossessedBy/LoadProgress`, `Private/Player/AuraPlayerController.cpp::PawnLeavingGame`, `Public/Combat/AuraCombatTypes.h`, `Private/Combat/AuraDeathPolicyDispatcher.cpp`, `Private/Player/AuraPlayerState.cpp` and their declarations. Extend Day 42's possession and disconnect hooks; do not add a later post-spawn correction path.
- New: `Source/Aura/Public/Gameplay/AuraRunRecoveryComponent.h` and implementation; extend mission snapshot, objective interaction leases and `GameplayCombatTuning.json`.
- Existing player HUD/targeting interfaces and gameplay network probe; add a registered recovery beacon using mission interaction primitives.

## Data and authority contract

Do not add a global Downed life enum. Existing death dispatch happens once; extend Day 42's mission death adapter with AwaitingRescue and keep legacy automatic respawn disabled for this profile. Two-player runs share two recovery charges; solo gets one automatic checkpoint recovery. A live connected teammate channels a nearby beacon for 3s. Completion reserves one charge and a one-use recovery receipt; only verified pawn/ASC attachment commits that charge and publishes Alive at 35% health/50% mana, with completed ammo unchanged and augments reattached once. Failed creation consumes no charge. Full two-player wipe means no Alive participant or retained live proxy remains and fails once; no automatic team revive. Solo first death attempts checkpoint recovery after 3s, second fails after its charge has been consumed.

Day 42's targetable dormant pawn/proxy remains damageable during the 60-second disconnect lease and retains health, DoTs, committed attack membership and absolute cooldown deadlines. Reconnect never heals, revives or resets a cooldown. A dead connected spectator plus a live disconnected proxy waits within that bounded lease; only new admissions suspend when nobody connected is Alive. Existing combat and the run deadline continue. Accepted proxy death or committed lease expiry recomputes wipe. Explicit member abandonment forfeits only that member and does not erase a survivor's progress.

## Numbered implementation steps

1. Intercept gameplay-profile recovery scheduling after accepted death, cancel reload/active abilities and record death sequence; preserve legacy auto-respawn behavior outside the profile.
2. Create a beacon tied to RunId/member/death sequence at a surveyed safe recovery anchor. Public data exposes role/status, not profile keys or private economy.
3. Reuse validated channel ownership/range/LOS/cancel rules. Reserve the selected charge under a recovery receipt before attempting creation so concurrent completions cannot spend it; finalize the decrement and Alive transition only after attachment succeeds. Release the reservation on terminal failure without a second charge.
4. Respawn through the existing server pawn factory with Day 42's mission attachment branch inside `PossessedBy/LoadProgress`, before legacy defaults/profile application, save or `MarkCombatReady`. Restore retained ammo/choices/supplies/cooldown deadlines and bind the ASC once. Verify the new pawn and ability state before publishing Alive; `RestartPlayer` returning is not evidence of success, and no default-health/ammo window may replicate.
5. Retain the authority avatar/ASC/profile independently of a disconnected controller before `PawnLeavingGame`/logout teardown. Handle dead-member reconnect, requester death during channel, beacon loss and zero charges. At expiry use Day 42's atomic hub restoration/marker clear before releasing profile ownership; preserve a Forfeited tombstone and never revive an expired member into the old run. A surviving player may proceed alone; dead members remain spectators and do not block extraction.
6. Test full wipe at the same server tick, solo last-charge death, failed possession, retries, live-proxy grace and disconnect during a committed attack; provide visible charges, AwaitingRescue and reconnect/forfeit status.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay51Tests.cpp`; namespace `Aura.Gameplay.Day51`.

- DeathDispatchOnce — lethal events create one death sequence and one beacon.
- RescueRaceSingleCharge — simultaneous completion/timer/retry consumes only one shared charge.
- RecoverRetainsRunState — ammo/augments/supplies persist without refill or duplicate effects.
- ReconnectDoesNotResurrect — a dead reconnecting participant remains AwaitingRescue.
- RecoveryAttachmentBeforeAlive — failed pawn creation or ASC/loadout attachment consumes no charge, never publishes Alive/default resources and never performs a legacy run-state save.
- DisconnectRetainsCombatAndCooldowns — windup/DoT can kill a dormant proxy; reconnect/repeated disconnect cannot heal or reset ability, evade or supply deadlines.
- ProxyWipeAndExpiry — dead spectator plus live proxy stays bounded by its lease; accepted proxy death or committed expiry terminates once, without reviving or charging twice.
- TeamWipeFails — both dead with unused charges still ends the run once.
- LegacyRecoveryUnchanged — old profile respawn and restart normalization pass.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 51 -Stage Fast -RunId d51-fast
./RunGameplayExpansion.ps1 -Day 51 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d51-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

RecoveryLab: same-role and mixed pairs, solo first/second deaths, survivor dies at 2.9s channel, dead participant reconnects at 10s/61s, live proxy hit during a committed windup/DoT, repeated disconnect during cooldown, dead spectator plus live proxy, rejected pawn/ASC attachment, channel target destroyed and simultaneous full wipe.

## Failure and timeout semantics

Recovery channel 3s; one respawn creation/attachment retry after 1s under the same receipt, then TechnicalRecoveryFailure abort with restored hub state. An uncommitted reservation is released, not spent; state stays Dead/Recovering until verified attachment. Reserved reconnect lease expires at 60s; failure to commit its hub restoration keeps that profile locked and ineligible for a new run until repair, without extending play eligibility. Orphan beacons expire with the run. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-51.json, death-rescue-charge-ledger.json, retained-loadout-snapshots.json, full-wipe/reconnect traces and live teammate rescue clip. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Players understand and execute a rescue; solo and pair failure rules are consistent; no double charge, default-ammo refill, effect duplication or bypass through reconnect.

## Defer / anti-goals

No downed crawling/bleedout system, reviving enemies, host migration, paid lives or changes to the shared combat life enum.
