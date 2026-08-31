# Day 46 — Role Kits and Combos

Status: Planned; not implemented by this planning job.  
Depends on: Day 45 status/interrupt owner, Day 44 ordered cancellation, Day 43 two-cell boundaries and Day 42 isolated run loadout/validated interaction intent.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Give Aura and BungeeMan distinct setup-and-payoff choices while preserving solo and same-role viability.

## Exact change surfaces

- Existing: `Content/Config/RoleConfig.json`, `Content/AbilityDefinitions/FireBolt.xml`, `FireBlast.xml`, `ArcaneShards.xml`, `Electrocute.xml`, `FireGun.xml`; `Source/Aura/Private/Player/AuraPlayerState.cpp`.
- Existing plugin interfaces: `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AbilityGraphTypes.h`, `DataAbility.h`, firearm/projectile action nodes; existing authoritative damage path.
- New: `Content/AbilityDefinitions/FocusShot.xml`, `ShockTrap.xml`; extend `GameplayRoleLoadouts.json`, `GameplayCombatTuning.json`, Day 45 status component and Day 42 run loadout owner.
- New: `Source/Aura/Public/Gameplay/AuraRunSupplyComponent.h` and private implementation, plus `Content/Config/GameplaySupplyDefinitions.json` containing minimal run medkit/emergency-terminal definitions. Extend Day 42 registered interaction intent and owner snapshot; Day 52 extends this same component/schema rather than replacing it.

## Data and authority contract

Existing legacy role definitions stay behavior-compatible; the gameplay profile grants a versioned run loadout. Electrocute contact for 0.5s and Shock Trap apply one Exposed stack for 3s. FireBolt/Focus Shot consume it once for 25% direct damage; FireGun/AoE do not. Focus Shot costs two existing rounds after a 0.4s windup, four-second cooldown; trap lasts 6s, cooldown 10s, radius 250, max three hostiles. Damage/effect handling stays server-owned and validates source run, role, life state and target.

Exposed has one authoritative record per target: owner participant ID, RunId, activation ID, monotonic application sequence and expiry. An accepted reapplication replaces the owner only for a later expiry, or the lower sequence at equal expiry; it never shortens expiry. The selected record owns support attribution and is removed atomically on consumption. This is the parameter seam used by Day 47's steady_hands.

Introduce two run-owned medkits per participant, each restoring 35% maximum health, and an emergency terminal at a reachable pad in each combat cell. The terminal requires a 3s uninterrupted, validated 200-unit/LOS/Alive channel and a 30s per-member cooldown, then grants six reserve rounds capped at 48 to BungeeMan or 20% maximum mana capped at maximum to Aura. Full-resource use rejects without consuming a charge/cooldown; interruption before commit grants nothing. Charges, receipts and absolute cooldowns survive same-server reconnect and are discarded at run end; persistent inventory is untouched. Day 52 adds packs/station/cache to this owner. No diagnostic refill, death or reconnect counts as the low-resource recovery path.

## Numbered implementation steps

1. Map existing Aura spells to clear burst/zone/channel roles; remove accidental overlap in tuning without replacing their graph architecture or legacy profile behavior.
2. Extend the firearm authority interface with bounded round-cost validation for Focus Shot; FireGun's single-round path remains unchanged. Validate creation/target before cost commit; rollback any reserved resources if execution cannot commit.
3. Implement Focus Shot and Shock Trap through graph interfaces and the status owner. A trap actor has RunId/activation identity and a per-target hit set; it cannot repeatedly expose via overlap jitter. Bind first trap hit and accepted ArcaneShards direct impact to the Day 45 Interrupt interface; generic hit-react alone is not an interrupt.
4. Add Exposed application/refresh ordering and consumption at the shared direct-damage resolution seam. Store owner/run/activation/sequence/expiry, never shorten expiry, resolve equal expiry with the lower server sequence, and atomically remove the selected record when calculating bonus damage. Retain selected-owner attribution for self-combo and cross-player support diagnostics.
5. Grant/revoke new specs under run handles, add cooldown/status icons immediately, and cancel winding/channeling abilities on evade/death/role teardown.
6. Add the minimal run supply owner/schema, two medkits, and one reachable emergency terminal per cell. Use prepare/commit/publish ordering for health/resource changes, validate ownership/range/LOS/Alive continuously during terminal use, and cancel on damage/accepted evade/disconnect. Display health charges and terminal cooldown immediately; maximum-resource rejection and reconnect cannot create a free claim.
7. Demonstrate self-combos for each role, cross-role combo, same-role pair and zero-ammo/mana terminal recovery. Complete Day 45's deferred Disruptor counter gate with actual Shock Trap and ArcaneShards player inputs in packaged play. Compare throughput and safe damage opportunities rather than demanding identical DPS.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay46Tests.cpp`; namespace `Aura.Gameplay.Day46`.

- ExposedConsumedOnce — simultaneous FireBolt and Focus Shot produce only one bonus consumption.
- ExposedReapplicationOrder — competing owners select later expiry then lower server sequence, never shorten expiry, and retain selected support attribution through atomic consumption.
- FocusShotAtomicCost — one round cannot fund a two-round shot; spawn failure consumes none.
- TrapTargetCap — overlap spam never exceeds three distinct exposed targets.
- OwnerRunGrantCleanup — new specs/statuses disappear on abort and never enter hub save.
- RoleParity — both roles complete every currently implemented archetype and the two-cell Clear slice; Day 49 later gates all three live templates.
- PlayerInterruptsDisruptor — actual BungeeMan Shock Trap and Aura ArcaneShards inputs interrupt a live channel with bounded cleanup; injected diagnostic events cannot satisfy this test.
- MinimalMedkitAtomicUse — two 35%-health run charges clamp to maximum; full health, duplicate request and precommit failure preserve charge and permanent inventory.
- EmergencyTerminalFromZero — zero-ammo BungeeMan and zero-mana Aura recover through the 3s/30s terminal with six reserve rounds/20% mana; interrupt/full-resource/cooldown retries grant nothing extra.
- MinimalSupplyReconnectState — reconnect retains remaining charges, claim receipts and absolute terminal cooldown without a default refill; run exit removes this state.
- LegacyFireGunRegression — semi-auto edges, reload, input lease, ammo HUD and Aura NotApplicable remain unchanged.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 46 -Stage Fast -RunId d46-fast
./RunGameplayExpansion.ps1 -Day 46 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d46-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

RoleComboLab: one Bulwark, one Disruptor, three clustered Raiders, fourth target outside trap cap, competing Exposed owners, zero ammo/mana, two medkits, reachable emergency terminals in both cells and death during windup. Include actual role-specific interrupt inputs, full-resource rejection, terminal interruption at 2.9s and reconnect during cooldown. All nine lanes and loss overlay.

## Failure and timeout semantics

Any invalid target, expired activation, wrong role, insufficient resource or missing definition rejects with typed reason. Channel/windup has explicit deadline plus one-second cancellation guard; no cost is consumed twice on retry. A medkit/resource apply failure preserves its charge/claim, full-resource use returns AlreadyFull unchanged, and an interrupted terminal does not start its 30s cooldown. Missing reachable terminal placement blocks low-resource acceptance. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-46.json, combo-consumption-ledger.json, role-loadout-diff.json, resource-before-after.csv, minimal-supply-receipts.json, zero-resource-terminal clips, both-role Disruptor interrupt clips, six self/team combo clips and legacy firearm results. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Both roles have a repeatable solo setup/payoff, contribute in pairs and demonstrate the real Disruptor interrupt counter. Simultaneous applications/consumers, projectile failures and teardown preserve resource/status invariants. Run medkits and both emergency terminals work through public inputs, including zero-resource recovery, with no persistent inventory mutation or reconnect refill; Day 47 can now test all eight augment consumers.

## Defer / anti-goals

No automatic FireGun mode, new weapons/classes, full weapon framework, arbitrary elemental reactions or mandatory healer/tank composition.
