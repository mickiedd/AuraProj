# Day 52 — Supplies and Optional Risk

Status: Inert contract foundation implemented; full runtime day remains Planned.
Depends on: Day 51 recovery, Day 48 interactions, Day 47 augments and Day 42 run inventory isolation.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

This increment adds run-only supply/cache/hazard reducers, closed definitions and all nine named native contract cases. Production actors, ASC damage application, mission snapshots, HUD and packaged play remain unwired behind inherited gates.

## Player outcome

Make supplies and an optional cache create readable risk/reward decisions without resource softlocks.

## Exact change surfaces

- Extend Day 46's `Content/Config/GameplaySupplyDefinitions.json` and `Source/Aura/Public/Gameplay/AuraRunSupplyComponent.h` with its private implementation; keep one supply owner. New: `Content/Config/GameplayHazardDefinitions.json`, `Source/Aura/Public/Gameplay/AuraGameplayHazard.h` and implementation.
- Extend run snapshots, registered mission interactions, Day 43 encounter recipes and Day 46 firearm/resource interfaces.
- Existing authoritative gameplay effect/damage nodes and native HUD; permanent `Content/Config/ItemDefinitions.json`/merchant inventory retain old hub behavior.
- Reuse `Source/Aura/Public/Combat/AuraCombatIdentityComponent.h`, `AuraCombatStateComponent.h`, `AuraCombatTypes.h` and `Private/AbilitySystem/AuraAbilitySystemLibrary.cpp::ApplyDamageEffect`. The new hazard actor owns the existing identity/state components and a source ASC with itself as avatar; use the existing ability-system interface for ASC lookup and `Public/Combat/AuraTargetableInterface.h` for its registered relay interaction as applicable. No Neutral faction or global combat-rule bypass is added.

## Data and authority contract

Extend the Day 46 supplies: each participant still starts with two run medkits and now also one resource pack, never copied from persistent inventory. Medkit heals 35% max health (45% with field_medic); pack gives BungeeMan +24 reserve rounds capped at 48 or Aura +50% max mana. At second cell boundary choose one station benefit: +1 medkit capped at two, or the resource refill; per-member receipt prevents double claim. Full-resource use rejects without consuming. Permanent health_potion cannot be consumed/traded in a run. Preserve the existing emergency terminal's risky 3s uninterrupted interaction every 30s per member for six reserve rounds or 20% mana. Charges, personal claims and absolute cooldown deadlines remain in the same authority owner across death and reconnect.

A vent is an authority-registered mission hazard source bound to RunId/epoch/activation, using the existing Enemy faction, a valid non-player combat identity with attack permission, and Alive combat state only after initialization. It owns a valid source ASC whose authoritative avatar is the hazard; the existing damage-effect boundary still validates source/target ASC, identity, life state, safe zones, friendly fire and civilian protection. It has no player controller/profile attribution and grants no player XP, loot, objective kill or reward credit. Disable/teardown invalidates its activation and pending hit callbacks. The relay interaction disables it; this is not a hostile roster kill. Do not introduce a neutral/unknown faction or bypass rule validation to make an environmental effect work.

## Numbered implementation steps

1. Extend the existing server-owned run supply counters and owner snapshots with packs/station receipts; validate source role, alive state, maximum capacities and request IDs before consume/refill. Loading the extended definitions must not reinitialize medkits or emergency cooldowns already owned by the run.
2. Reuse effect/resource APIs with prepare/commit/publish ordering; at full health/ammo/mana return AlreadyFull with unchanged charge. Death/cancel before commit consumes nothing.
3. Add station choices and revalidate each cell's existing emergency terminal pad. Terminal is not in a safe-zone immunity bubble during combat; damage cancels its channel. Preserve one per-member emergency cooldown across pads, so changing terminals or reconnecting cannot bypass the 30s limit.
4. Build optional cache branch: explicit team consent with a 10s decision timeout that defaults to decline, 30s finite challenge, extra roster within existing caps, one reward flag on success. Declining keeps primary path/extraction accessible. Record the flag now; permanent payout starts only with Day 53 settlement.
5. Add a vent hazard with 1s cue, 2s active window, 5s cooldown and server per-activation hit membership; disable via an exposed relay. Initialize the hazard's Enemy/non-player identity, attack permission, Alive state and source ASC/avatar before activation. Route damage through the existing authoritative effect boundary with target ASC and resolved run policy; reject invalid sources and protected targets, and never attribute a hazard outcome to a player or count relay disable as an enemy kill.
6. Compare taking supplies first versus cache first, including one exhausted build. Show costs and risk before confirmation; no hidden permanent currency spending.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay52Tests.cpp`; namespace `Aura.Gameplay.Day52`.

- SupplyAtomicAndCapped — full-resource and duplicate use preserve charge; successful use changes it once.
- RoleResourceApplicability — Aura receives mana and NotApplicable ammo, BungeeMan reserve only.
- StationReceiptOnce — reconnect/second client cannot claim the same personal refill twice.
- SupplyOwnerExtensionPreservesState — adding packs/stations preserves Day 46 medkit counts and emergency deadlines; changing pads or reconnecting cannot reset either.
- EmergencyResourceNoSoftlock — zero-resource player can recover through a cancellable terminal without permanent profile mutation.
- CacheOptionalAndBounded — failure or decline cannot block the main objective or exceed encounter caps.
- HazardRespectsRules — safe civilians/friendly-fire exclusions and per-activation hit limits apply.
- HazardRegisteredSourceDamage — a registered Alive Enemy-faction hazard with valid source ASC/avatar damages an eligible player once per activation; no player XP/loot/objective/reward credit is produced.
- HazardInvalidSourceFailsClosed — missing identity/ASC, non-Alive or stale-run/disabled source causes zero damage; safe-zone players and protected civilians remain protected without a new faction or rule bypass.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 52 -Stage Fast -RunId d52-fast
./RunGameplayExpansion.ps1 -Day 52 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d52-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

SupplyRiskLab: zero ammo/mana, nearly-full resources, cache accept/decline/timeout, simultaneous station access, death during use and hazard-on-escort boundary. Include one eligible player hit, protected civilian, safe-zone player, friendly Enemy, missing identity/source ASC, non-Alive source and disable/teardown racing an impact; all nine lanes.

## Failure and timeout semantics

Supply effect apply failure leaves charge intact. Terminal/station channel 3s and ordinary interrupt rules; cache times out after 30s, closes only optional branch and grants no cache flag. Hazard lifetime/callbacks cancel on run teardown. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-52.json, supply-receipts.json, risk-choice-results.csv, hazard-policy-timeline.json, empty-resource recovery clip and owner-only inventory captures. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Players make an informed supply/risk choice, neither role is permanently resource-locked, optional content never blocks success and no run supply enters permanent inventory.

## Defer / anti-goals

No crafting, random item drops, persistent consumable rebalance, weapon durability, currency trading or environmental destruction simulation.
