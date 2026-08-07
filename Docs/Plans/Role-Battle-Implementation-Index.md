# Role and Battle System: Daily Implementation Index

Status: Days 01-02 complete; Day 03 is next

This is the execution schedule for Role-Creation-and-Battle-System-Plan.md. Each day has its own file with source files, exact work items, verification steps, and a completion gate.

The schedule assumes one developer working in a stable Unreal project branch. A “day” is a logical milestone; it may take more or less than one calendar day.

## How to use this schedule

1. Read the current day file before editing code.
2. Confirm that the previous day’s completion gate passes.
3. Make only the changes listed for the current day.
4. Compile and run the listed checks before moving on.
5. Commit the day as one reviewable unit.
6. Record deviations in the day file or the project tracking document.

Do not skip the gates. The most important dependency is that faction-aware damage must work before Civilian AI and civilian death are added.

## Daily sequence

| Day | Milestone | Main result |
| --- | --- | --- |
| 01 | Baseline | Existing Aura/BungeeMan behavior is recorded and reproducible |
| 02 | Combat identity | Actors have replicated faction, profile, control, and death-policy identity |
| 03 | Combat rules | One centralized relationship and target-permission service exists |
| 04 | Damage migration | Every damage producer uses the centralized rules |
| 05 | Role schema | RoleConfig supports explicit role, equipment, combat, and behavior data |
| 06 | Role application | Role loadouts are cleared and applied deterministically |
| 07 | Combat role regression | Aura and BungeeMan work through the new role path |
| 08 | Civilian actor | AAuraCivilian exists with health and non-combat configuration |
| 09 | Civilian spawning | Civilians can be spawned from a population definition |
| 10 | Civilian AI | Civilians work, wander, flee, and seek shelter |
| 11 | Death lifecycle | Player, enemy, and civilian death policies are separated and idempotent |
| 12 | Battle director | Conflict zones and battle phases control relationships and events |
| 13 | Population lifecycle | Civilian and enemy population refill/cleanup is authoritative |
| 14 | Targeting and interaction | Civilian targeting and interaction are distinct from enemy targeting |
| 15 | Economy data | Items, offers, and commerce types are defined and validated |
| 16 | Wallet and inventory | Server-authoritative wallet and inventory components work |
| 17 | Merchant transactions | Civilian merchant purchases are validated and atomic |
| 18 | Persistence | Role, wallet, inventory, and population state save and reload |
| 19 | Multiplayer hardening | Dedicated/listen-server acceptance matrix passes |
| 20 | Cleanup and release | Old assumptions are removed and the first vertical slice is documented |

## Recommended commit names

- Day 01 - Establish role battle baseline
- Day 02 - Add combat identity
- Day 03 - Add combat relationship rules
- Day 04 - Route damage through combat rules
- Day 05 - Extend role schema
- Day 06 - Refactor role application
- Day 07 - Validate Aura and BungeeMan roles
- Day 08 - Add civilian actor
- Day 09 - Add civilian population spawning
- Day 10 - Add civilian behavior
- Day 11 - Separate death lifecycle
- Day 12 - Add battle director
- Day 13 - Add population lifecycle
- Day 14 - Add targeting and interaction
- Day 15 - Add economy definitions
- Day 16 - Add wallet and inventory
- Day 17 - Add merchant transactions
- Day 18 - Add persistence
- Day 19 - Harden multiplayer behavior
- Day 20 - Complete vertical-slice cleanup

## Important project rules

- Do not add Civilian to ECharacterClass.
- Do not use new raw Player/Enemy/Civilian actor-tag checks.
- Do not infer weapon ownership from the presence of an LMB ability.
- Do not grant role abilities from client input.
- Do not use AAuraEnemy for civilian behavior or civilian death.
- Do not implement runtime role switching until role-granted ASC cleanup exists.

## Detailed day files

- [Day 01 - Establish the baseline](Role-Battle-Implementation/Day-01-Baseline.md)
- [Day 02 - Add combat identity](Role-Battle-Implementation/Day-02-Combat-Identity.md)
- [Day 03 - Add combat relationship rules](Role-Battle-Implementation/Day-03-Combat-Rules.md)
- [Day 04 - Migrate all damage producers](Role-Battle-Implementation/Day-04-Damage-Migration.md)
- [Day 05 - Extend the role schema](Role-Battle-Implementation/Day-05-Role-Schema.md)
- [Day 06 - Refactor role application](Role-Battle-Implementation/Day-06-Role-Application.md)
- [Day 07 - Validate Aura and BungeeMan](Role-Battle-Implementation/Day-07-Combat-Roles.md)
- [Day 08 - Add the Civilian actor](Role-Battle-Implementation/Day-08-Civilian-Actor.md)
- [Day 09 - Add civilian spawning](Role-Battle-Implementation/Day-09-Civilian-Spawning.md)
- [Day 10 - Add civilian AI](Role-Battle-Implementation/Day-10-Civilian-AI.md)
- [Day 11 - Separate death lifecycle](Role-Battle-Implementation/Day-11-Death-Lifecycle.md)
- [Day 12 - Add the battle director](Role-Battle-Implementation/Day-12-Battle-Director.md)
- [Day 13 - Add population lifecycle](Role-Battle-Implementation/Day-13-Population-Lifecycle.md)
- [Day 14 - Add targeting and interaction](Role-Battle-Implementation/Day-14-Targeting-Interaction.md)
- [Day 15 - Add economy definitions](Role-Battle-Implementation/Day-15-Economy-Data.md)
- [Day 16 - Add wallet and inventory](Role-Battle-Implementation/Day-16-Wallet-Inventory.md)
- [Day 17 - Add merchant transactions](Role-Battle-Implementation/Day-17-Merchant-Transactions.md)
- [Day 18 - Add persistence](Role-Battle-Implementation/Day-18-Persistence.md)
- [Day 19 - Harden multiplayer](Role-Battle-Implementation/Day-19-Multiplayer-Hardening.md)
- [Day 20 - Finish the first vertical slice](Role-Battle-Implementation/Day-20-Cleanup-Release.md)

