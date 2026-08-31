# Gameplay analysis and design rationale

Date: 2026-08-31. Method: source/config inspection, existing reports, and primary-source game/design research. No hands-on claim is made for AuraProj or the comparison games in this planning task. Conclusions about what will be fun are hypotheses to test.

## What AuraProj already offers

| Observed surface | Present capability | Gameplay implication / missing proof |
| --- | --- | --- |
| `Content/Config/RoleConfig.json` | Aura starts with FireBolt, FireBlast, ArcaneShards, Electrocute; BungeeMan starts with FireGun | Aura already has breadth to tune. BungeeMan needs decisions beyond shooting/reloading, not another full weapon framework. |
| `Content/AbilityDefinitions/Electrocute.xml` | Targeting, beam selection, mana cost, cooldown, timed montage fallback | Existing graphs can carry intentional combos, but UI readability and live counterplay need testing. |
| `Source/Aura/Private/Character/AuraEnemy.cpp` | Class abilities, damage/death, data-driven loot and XP; regular BT and BehaviorU component paths coexist | Reuse enemies; explicitly select one active AI driver per mission enemy before adding tactics. Attachment alone does not prove simultaneous execution in every map. |
| `Source/Aura/Private/Actor/AuraEnemySpawnPoint.cpp` and `AuraEnemySpawnVolume.cpp` | Overlap-triggered spawn points | These functions lack local authority/null guards. A repeatable encounter needs checked spawn leases, cancellation, and accounting. Runtime severity still needs reproduction. |
| `Source/Aura/Public/Battle/AuraBattleDirector.h` | Battle phase and location-based combat policy; replicated population summary | It is a policy owner, not a mission/objective/pacing director. Do not grow it into the owner of every gameplay system. |
| `Source/Aura/Public/World/AuraPopulationManager.h` | Civilian-only population, stable member IDs, reservations, recovery timers | Rescue can reuse civilians and markers; hostile waves must not be inserted into this non-combat population manager. |
| `Source/Aura/Private/Game/AuraGameModeBase.cpp::GrantCivilianLethalReward` | Hard-coded 25-gold credit with an in-memory actor-ID/death-sequence receipt set | Fine-grained reward configuration and restart-proof mission settlement are not established by this path. Civilian farming is an unsuitable main mission incentive. |
| `Source/Aura/Public/Player/AuraPlayerState.h` | Stable ASC, firearm state, wallet/inventory, tutorial and recovery state | Good lifetime owner, but run augments and refilled ammo must not silently enter persistent profiles. |
| `Source/Aura/Public/Game/AuraPersistenceSubsystem.h` | Profile/world checkpoint manifest and identity validation | Extend existing commit boundaries for run settlement; do not invent a second save system. |
| `Plugins/AuraWebUI/Content/WebUI/hud-bottom.html`, `hud-left-top.html`, `hud-right-top.html` | Existing multi-panel HUD | Add compact objective, threat and build views; retain native authority and avoid a second simulated state in JavaScript. |

This project has many systems, but system count is not decision density. The highest-value gap is the absence of a compact, repeatable experience that asks players to prioritize, react, cooperate, choose, and evaluate a result.

## Lessons from comparable games

| Reference and supported observation | Design inference for AuraProj | Deliberately not copied |
| --- | --- | --- |
| **Hades:** the developer's store description emphasizes ability-changing boons, repeated attempts, and escalating challenge. [Developer-authored Steam page](https://store.steampowered.com/app/1145360/Hades/) | Offer two small, meaningful build decisions per run. Let players recognize a combo and test another combination next run. Test behavior changes, not just DPS inflation. | Hundreds of boons, huge narrative production, permanent stat grind, or claims that our eight augments have thousands of balanced builds. |
| **Deep Rock Galactic:** its official description foregrounds teamwork, varied caves, hostile encounters, equipment, and objectives. [Official game site](https://www.deeprockgalactic.com/) | Give players a shared job with different tactical contributions; clear readable objectives and recovery encourage cooperation. Recombine authored arrangements rather than making a terrain generator. | Destructible cave technology, four new classes, expansive biomes, or an assumption that more players automatically make combat better. |
| **Left 4 Dead:** Valve's AI presentation describes player-intensity estimation and alternating build-up, peaks, and relief. [Michael Booth, Valve, 2009](https://cdn.akamai.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf) | Separate encounter scheduling from enemy behavior. Use bounded pressure and visible recovery windows; modulate incoming threats rather than secretly changing enemy HP mid-fight. | Valve's complete AI stack, huge hordes, invisible unavoidable spawns, or a claim to measure players' emotions precisely. |

These comparisons justify testable design directions, not numerical balance settings. All numbers below and in daily plans are AuraProj starting targets; none are reported as measured facts or settings taken from those games.

## Fun hypotheses and rejection tests

| Hypothesis | Concrete player decision | Evidence that could disprove it | Delivery |
| --- | --- | --- | --- |
| Counterplay is more satisfying than target clicking | Sidestep a Lancer, flank a Bulwark, interrupt a Disruptor | Players cannot identify why damage occurred or use the same behavior against every enemy | 44–46, 59 |
| Different roles create cooperation without dependency | Aura exposes a target; BungeeMan spends a shot window, either role can self-combo | Same-role or solo lanes are blocked, or one role only watches | 46, 51, 59 |
| Build choice changes a run | Take a stronger evade, wider trap, or safer channel | Players cannot describe a choice's effect; one choice always dominates | 47, 59 |
| Objectives make space matter | Protect a moving civilian or commit to an exposed relay interaction | Every mission becomes stationary enemy clearing; pathfinding creates dead time | 48–49 |
| Relief improves combat pacing | Use a break to choose an augment and resupply | More than 30 seconds of aimless waiting, or nonstop spawning masks decisions | 50, 52 |
| Optional risk creates stories | Start a bounded cache challenge before extraction | Best strategy is always take/always ignore; danger is unclear before acceptance | 52, 55 |
| Fair rewards improve cooperation | Help the shared objective instead of stealing kills | Last-hit farming, AFK rewards, or civilian kills outperform mission completion | 53 |
| A finale motivates another attempt | Learn three boss patterns and try a different build | Boss is only an HP sponge, unavoidable damage, or repetitive immunity waits | 54, 59 |

## Proposed combat and content roster

**Raider:** closes distance, telegraphs a 0.65-second melee strike, then recovers for 0.8 seconds. Answer: evade and punish; no perfect tracking after the strike commitment.

**Lancer:** holds range and shows a 1.0-second line attack; locks direction for the final 0.35 seconds. Answer: break sight or sidestep; no hidden hitscan damage before the cue.

**Bulwark:** reduces frontal damage by 60% within a 120-degree front arc; vulnerable flanks and 1.2-second recovery after a slam. Answer: reposition or use a deliberate expose window; no absolute immunity that makes solo impossible.

**Disruptor:** channels a visible 2-second support field for nearby hostiles. Answer: interrupt or prioritize; field radius 600 Unreal units, at most two buffed allies, no buff stacking between Disruptors.

**Captain boss:** telegraphed sweep, line charge, and interruptible reinforcement channel; transition at 50% health, preserve counter windows, cap simultaneous adds at four. Same-role and solo combat must remain viable. Reuse silhouettes/animations where readable; budget one small new cue family, not a cinematic asset campaign.

**Aura:** keep FireBolt as primary, FireBlast as close burst, ArcaneShards as zone pressure, and Electrocute as a sustained expose opportunity. **BungeeMan:** retain semi-auto FireGun plus Focus Shot (precision spender) and Shock Trap (setup/control). Both can expose with their own setup and spend through their primary attack; cross-role execution is a convenience, not a requirement.

Eight augments are fixed in the architecture document with limits and concrete tests. Their roles are mobility, safety, timing, and control. Rarity tiers, random affixes, permanent combat levels, and branching talent trees are unnecessary for this slice.

## Architecture optimizations ranked by player impact

1. **One authoritative run and one AI driver:** remove conflicting behavior/lifetime owners so objectives and enemies are predictable.
2. **Reusable encounter and objective contracts:** a designer can vary a mission without a new GameMode branch or arbitrary RPC.
3. **One combat-resolution boundary:** telegraphs, statuses, augments, and boss patterns reuse server damage and effect rules; presentation never grants a hit.
4. **Persistent versus run state:** prevent replay exploits and preserve the player's old profile while enabling freely reset builds and supplies.
5. **Measured scheduling and replication:** cap active enemies and expensive target searches, coalesce HUD updates, profile before considering pooling or replication-system replacement.

Epic documents attribute/effect ownership in GAS and reducing unnecessary replication/update frequency. These support reusing the current ASC/effects and profiling focused networking changes; they do not justify a wholesale rewrite. [UE 5.5 Gameplay Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine?application_version=5.5), [UE 5.5 bandwidth guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/performance-and-bandwidth-tips-for-unreal-engine?application_version=5.5).

## Honest evaluation

Automated tests can prove that an evade is bounded, a reward is idempotent, or a boss cue precedes damage. They cannot prove that the game is enjoyable. Human observation must include hesitation, understandable deaths, voluntary replay, build explanations, same-role cooperation, and objections. Report counts and denominators; do not convert a six-person internal sample into a retention forecast. Keep a failure clip and a negative quote alongside positive outcomes when participants consent to recording.
