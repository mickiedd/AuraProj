# Day 44 — Telegraphs and Evasion

Status: Planned; not implemented by this planning job.  
Depends on: Day 43 authoritative enemy lifecycle.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Players can recognize incoming attacks and avoid them through a deliberate, responsive defensive action.

## Exact change surfaces

- Existing: `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AbilityGraphTypes.h`, `Private/Nodes/Actions/EnemyMeleeDamageNode.cpp`, `Source/Aura/Private/Player/AuraPlayerController.cpp`, character movement and existing input bindings.
- New: `Source/Aura/Public/Gameplay/AuraAttackTimelineComponent.h`, `AuraEvadeComponent.h`, matching implementations and `Content/AbilityDefinitions/Evade.xml`.
- Content: `Content/Config/GameplayCombatTuning.json`, existing input config asset `Content/Blueprints/Input/DA_AuraInputConfig.uasset`, native HUD and `Plugins/AuraWebUI/Content/WebUI/hud-bottom.html`.

## Data and authority contract

AttackId, cue shape, start/impact server time and cancellation are authoritative replicated state. The cue never causes damage. Heavy attacks have at least 0.85 seconds of server windup. For viewers relevant before Windup starts, at least 95 of 100 attacks must provide at least 0.65 seconds of rendered cue lead in the 150ms RTT/2% loss lane; report late-relevance viewers separately with only their remaining lead. A cancelled/dead attacker cannot hit later. Evade is 350 units over 0.25 seconds, cooldown 2.5 seconds, one charge, swept collision and no invulnerability. Validate owner/life/cooldown/direction first; an accepted request cancels its owner's interaction/channel/reload and commits cooldown before movement. Fully blocked movement still consumes that cooldown; invalid/duplicate requests cancel nothing. Resolve the default key against existing bindings and record it in the scope; rebinding is preserved. Client sends a normalized direction, never destination or elapsed time.

## Numbered implementation steps

1. Add timeline phases Windup/Committed/Recovery/Cancelled and explicit generation guards around impact callbacks; use server fallback timing when dedicated montage notifies are absent.
2. Render directional/area cues with shape, motion and audio/text alternatives. On late relevance compute remaining windup; expired cues cannot replay damage or hide current attacks.
3. Implement evade through existing movement collision APIs. Validate owner, Alive/run eligibility, cooldown and finite normalized direction before accepting; acceptance cancels the owner's interaction/channel/reload, commits cooldown, then attempts the clamped swept movement. Reject wall penetration; a blocked sweep retains the accepted cooldown/cancellation, while invalid/duplicate requests change neither.
4. Provide immediate input feedback and authoritative cooldown/cancel result. Do not globally pause time/hit-stop on a multiplayer server; local camera shake remains optional.
5. Instrument actual rendered cue visibility separately from server start/impact timestamps and movement corrections in the 150ms RTT/2% loss lane. Record all 100 attacks for pre-relevant viewers and a separate late-relevance cohort. Deliberately delay delivery in a diagnostic case and report degraded lead rather than relabeling server timestamps as visual evidence.
6. Play a basic Warrior's heavy-strike prototype in both roles and collect intentional evade/punish sequences plus a failure clip showing understandable damage. Day 45 reuses this timeline for the Raider archetype; no Day 45 asset is a prerequisite here.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay44Tests.cpp`; namespace `Aura.Gameplay.Day44`.

- CuePrecedesImpact — server windup is >=0.85s and rendered pre-relevant viewers show >=0.65s lead on at least 95 of 100 attacks at 150ms RTT/2% loss; server and rendered observations remain separate.
- DeadAttackerCancelsImpact — lethal damage during windup produces no delayed hit.
- EvadeBlockedByWall — a wall at 150 units truncates the 350-unit displacement without tunneling.
- EvadeRequestAbuse — oversized vectors, duplicate requests and rapid retries cannot exceed one charge.
- EvadeCancelsReload — BungeeMan retains completed ammo and the old reload callback cannot finish.
- AcceptedEvadeCancelsInteraction — accepted evade cancels the owned channel and commits cooldown even when its sweep is fully blocked; invalid/duplicate requests cancel nothing.
- DelayedCueDeliveryReported — forced delivery delay records reduced rendered lead without rewriting attack time or counting the diagnostic case as normal-cohort success.
- LateJoinCueTime — spectator sees remaining cue time without a second attack.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 44 -Stage Fast -RunId d44-fast
./RunGameplayExpansion.ps1 -Day 44 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d44-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

CounterplayLab: one telegraphed basic Warrior heavy-strike prototype, wall corridor, fully blocked sweep during an interaction, invalid/duplicate evade while channeling, two players on opposite sides and high/low frame-rate capture; all nine lanes plus the network-failure overlay. Include 100 heavy attacks for pre-relevant viewers, a separate late-relevance cohort and a deliberately delayed cue.

## Failure and timeout semantics

Attack callback timeout is its definition deadline plus one second, then Cancelled. No montage wait can hang; rejected evade shows a typed reason within the shared two-second UI window. Missed visual assets block rendered acceptance. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-44.json, attack-timeline.json, movement-sweeps.json, latency-corrections.csv and rendered before/after counterplay clips. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Both roles can deliberately avoid and punish the fixture, no client bypasses collision/cooldown, ordered evade cancellation is consistent, and rendered cue lead meets the explicit 95-of-100 threshold under required latency with late/delayed viewers reported honestly.

## Defer / anti-goals

No invulnerability frames, parry system, rollback combat, global hit-stop or bespoke movement prediction rewrite.
