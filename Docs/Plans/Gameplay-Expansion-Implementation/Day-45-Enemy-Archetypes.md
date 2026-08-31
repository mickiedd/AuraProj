# Day 45 — Enemy Archetypes and Counters

Status: Planned; not implemented by this planning job.  
Depends on: Days 43–44; basic status/interrupt interface is introduced here for reuse on Day 46.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Ship four enemies that require different responses rather than just different health values.

## Exact change surfaces

- Existing: `Source/Aura/Private/AI/BTTask_Attack.cpp`, `BTTask_MoveToTarget.cpp`, `BTService_FindNearestHostile.cpp`, `Private/Character/AuraEnemy.cpp`; `Content/Config/EnemyAbilityConfig.json`.
- Modify planned Day 43 `AuraEnemyArchetypeComponent`/definitions and `AuraEncounterCoordinator` token ownership, plus Day 44 `AuraAttackTimelineComponent`; add `Source/Aura/Public/Gameplay/AuraCombatStatusComponent.h` and implementation.
- Content: new mission-only attack XML definitions for Raider, Lancer, Bulwark, Disruptor under `Content/AbilityDefinitions/`; explicit combat-tree references and tuning rows in `GameplayEnemyArchetypes.json`.

## Data and authority contract

Raider commits after at least 0.85s, Lancer shows a 1s line with aim and damage shape frozen for the entire Windup, Bulwark has a 120-degree/60% frontal reduction and 1.2s slam recovery, Disruptor has a 2s interruptible support channel granting 20% incoming-damage reduction to at most two allies within 600 units. The buff expires no later than channel end and is removed on cancel/death/range departure. Overlapping fields share one effect owned by the lowest live source lease ID; reevaluate ownership when that source ends. Effects use GAS and the shared combat boundary. Use one minimal Interrupt event with two-second channel immunity; player-generated interrupt bindings arrive Day 46, so Day 45's injected interrupt evidence is diagnostic only.

EncounterCoordinator owns heavy-attack admission tokens before Day 50 pacing exists. At Windup acquire one token for each live player/proxy within 600 units of the committed attack shape, maximum two per affected participant; a multi-target attack must acquire every required token or requeue. Coverage is a snapshot of participants at Windup, not a promise to prevent a player walking into an already telegraphed attack. The damage shape remains frozen; invalid attacker state cancels the attack. Release on impact/cancel/death, no later than Recovery exit, with encounter-generation cleanup. Ban more than two simultaneous Disruptors. No per-archetype damage bypass or independent token owner is permitted.

## Numbered implementation steps

1. Implement explicit Idle/Approach/Windup/Attack/Recovery action data per archetype on the chosen BT driver; cache validated definitions outside tick.
2. Freeze Lancer aim/shape from Windup start through impact; add authoritative shape/frontal-angle checks respecting cover, faction, life state and registered targets. A target moving after the line appears cannot drag the line or damage shape with it.
3. Implement Disruptor's bounded 20% reduction channel with a tracked effect handle set and lowest-live-lease ownership for overlapping sources. Channel end/death/cancel/range departure removes only its own buffs; handover reevaluates the remaining live source and cannot stack or extend beyond its deadline.
4. Create encounter recipes that teach each enemy alone, then two mixed groups. Enforce the two-Disruptor cap and add encounter-owned heavy tokens at Windup for every live player/proxy within 600 units of the committed shape. Acquire all affected participants' tokens atomically, requeue if any cap is full, and release on impact/cancel/death/generation teardown. Day 50 consumes this existing admission interface.
5. Use existing meshes/animations with readable silhouette/cue labels; document missing art and reject indistinguishable enemy presentation in visual QA.
6. Run solo/same-role/mixed lanes and note whether the available movement/flank counters appear in play; adjust behavior before HP. Exercise Disruptor Interrupt through the accepted server fixture interface and label it diagnostic. Day 46 must produce actual Aura and BungeeMan interrupt clips before claiming both-role Disruptor counterplay.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay45Tests.cpp`; namespace `Aura.Gameplay.Day45`.

- RaiderCommitStopsTracking — strike direction does not follow a post-commit dodge.
- LancerCoverBlocksDamage — target behind geometry takes no line-attack damage.
- LancerWindupShapeFrozen — moving the target throughout the one-second Windup leaves the telegraph/damage shape and Windup token-coverage snapshot unchanged.
- BulwarkFrontVersusFlank — 60% reduction applies only inside front arc; solo can punish recovery.
- DisruptorInterruptCleanup — diagnostic accepted Interrupt/death removes buffs and ends the two-second channel exactly once; do not count it as player-input evidence.
- SupportFieldNoStack — two overlapping Disruptors provide one 20% reduction, lowest-live-lease ownership, bounded handover and no buff beyond source channel/range validity.
- MixedGroupThreatCap — encounter tokens cap heavy Windups at two per affected live player/proxy within 600 units of the committed shape, atomically acquire multi-target budgets and release after impact/cancel/death/teardown.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 45 -Stage Fast -RunId d45-fast
./RunGameplayExpansion.ps1 -Day 45 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d45-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

ArchetypeSchool: each archetype alone and groups Raider+Lancer, Bulwark+Disruptor; clear walls, flanking route, unreachable target, controller loss, overlapping Disruptor sources and multi-target heavy admission including a dormant proxy; all nine lanes. Interrupt injection is diagnostic until the Day 46 real player bindings pass.

## Failure and timeout semantics

Path failure retries once after one second; if still unreachable, coordinator moves to a different validated target/anchor or aborts a required encounter within five seconds. Missing tree/action definition fails entry. Cancelled support handles may not survive teardown. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-45.json, archetype-counter-results.csv, cue screenshots, support-handle counts, heavy-token-ledger.json, path-failure traces and available live counterplay clips; retain a separately labeled Disruptor interrupt diagnostic and an outstanding Day 46 both-role input gate. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Raider/Lancer/Bulwark counters are visibly demonstrated by both roles; Disruptor channel, bounds and diagnostic interrupt cleanup pass with real player interruption explicitly gated on Day 46. All action/cancel paths terminate, encounter-owned heavy budgets include dormant proxies, and no mission requires a particular role to damage an enemy.

## Defer / anti-goals

No new skeleton production, flying/nav-special enemies, elemental resistance matrix, faction simulation, or HP-only variants counted as new enemies.
