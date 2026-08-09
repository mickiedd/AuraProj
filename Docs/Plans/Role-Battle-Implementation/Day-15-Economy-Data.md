# Day 15 - Add Economy Definitions

## Goal

Create one versioned, immutable economy registry that validates items, offers, merchants, currency, inventory limits, and population-to-merchant bindings as a single unit before any runtime economy state exists.

The authoritative server registry is the only source of accepted item IDs, offer contents, prices, stock policy, and eligibility rules. Client JSON or UI values are presentation only and are never transaction input.

## BungeeMan Gun Skill checkpoint

Keep FireGun, `fireGunBullet`, cooldown, damage, and weapon definitions outside the economy registry. Merchant bindings, prices, item IDs, and currency validation must not become inputs to the gun ability or a way to mutate economy state through damage.

## Prerequisite gate

- Day 09 provides versioned population rows, stable `PopulationId:SlotIndex` member IDs, and syntax-checked optional per-slot `memberOverrides[].merchantDefinitionId` values.
- Day 13 owns the stable Civilian slot registry and snapshot; Day 14 exposes Trade only as a disabled interaction option until economy validation succeeds.
- The Day 14 native, listen, dedicated, and late-join gates pass before economy files are introduced.

## New files

- Content/Config/ItemDefinitions.json
- Content/Config/MerchantDefinitions.json
- Content/Config/EconomyConfig.json
- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraEconomyConfig.h
- Source/Aura/Private/Economy/AuraEconomyConfig.cpp
- Source/Aura/Public/Economy/AuraEconomyRegistrySubsystem.h
- Source/Aura/Private/Economy/AuraEconomyRegistrySubsystem.cpp
- Source/Aura/Private/Tests/AuraEconomyConfigTests.cpp
- RunRoleBattleDay15NetworkSmoke.ps1

## Files to modify or verify

- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/World/AuraPopulationSpawnDefinition.h
- Source/Aura/Private/World/AuraPopulationSpawnDefinition.cpp
- Source/Aura/Public/World/AuraPopulationTypes.h
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp
- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Content/Config/PopulationSpawnTable.json
- Config/DefaultGame.ini

## Data contract

1. Give every economy JSON root a required `schemaVersion`. All three files must use a supported, mutually compatible version.
2. Use arrays with explicit IDs so duplicate item, offer, merchant, and currency IDs can be detected rather than hidden by duplicate JSON object keys.
3. Use `FName` or Gameplay Tags for IDs. IDs are case-normalized according to one documented rule before duplicate and reference checks.
4. Use `int64` end to end for currency balances, prices, item quantities, stack limits, and finite stock. Do not convert through `float` or `int32`.
5. Encode `int64` JSON values as canonical base-10 strings, then parse with checked overflow handling. Reject signs where a value must be non-negative, fractional notation, whitespace, non-canonical text, and overflow. This avoids loss through JSON's floating-point number representation.
6. Define item fields:
   - Item ID.
   - Display name and category.
   - Stack limit greater than zero.
   - Validated gameplay effect or use-action reference, if applicable.
7. Define offer fields:
   - Offer ID.
   - Item ID.
   - Item grant quantity greater than zero.
   - Authoritative buy price.
   - Initial stock and explicit finite/unlimited stock policy; never use a negative sentinel.
   - Optional required role and interaction tags.
8. Define merchant fields:
   - Merchant definition ID.
   - Offer IDs.
   - Stock policy overrides, if any.
   - Optional allowed civilian work profiles.
   - Open-schedule placeholder with no runtime effect until a schedule policy is implemented.
9. Define `EconomyConfig` fields:
   - Schema version.
   - The one supported currency ID for this slice.
   - New-profile starting balance.
   - Optional maximum wallet balance, otherwise `MAX_int64`.
   - Maximum inventory item slots.
10. Define capacity as physical item slots, not distinct item IDs, total units, or weight. One slot contains one item ID and a quantity from one through that item's stack limit. Adding fills existing partial stacks in stable order before allocating new slots; removing drains stacks in reverse stable order. The number of occupied stacks may never exceed `maxItemSlots`.
11. Omit item sell values and offer sell prices in this slice. Selling requires its own immutable offer and transaction policy later.

## Merchant binding contract

1. Consume the optional `merchantDefinitionId` introduced by Day 09 only from the resolved per-slot `memberOverrides` entry. Empty means that exact member is not a merchant; Day 15 upgrades the earlier syntax-only check to a cross-registry reference check. Never read a row-wide merchant default.
2. Copy the validated JSON `merchantDefinitionId` into runtime `MerchantDefinitionId` and preserve Day 09's authority-assigned `PopulationId`, `PopulationSlotIndex`, and canonical `PopulationMemberId` (`PopulationId:SlotIndex`) on the spawned Civilian. Replicate the resolved read-only binding needed for presentation.
3. Treat a role-level `economyProfile` only as a capability/default classification. It must not automatically turn every actor using the shared Civilian role into a merchant.
4. Validate every non-empty per-member `merchantDefinitionId` against the economy registry and any allowed work-profile constraint before population spawning. Reject duplicate slot overrides, ambiguous inheritance, or missing bindings.
5. Canonical `PopulationMemberId` is the persistence key for per-instance stock; actor names and transient network GUIDs are not persistence keys.

## Registry and startup sequence

1. `UAuraEconomyRegistrySubsystem` derives from `UGameInstanceSubsystem` and owns the published read-only snapshot for the process lifetime. Its authoritative load/publish entry point rejects client worlds. Mutation APIs are private to loading/tests; gameplay callers receive const lookups or a lifetime-safe immutable snapshot handle.
2. Parse all three files and the population merchant references into temporary structures.
3. Validate the complete temporary snapshot:
   - Supported and compatible schema versions.
   - Duplicate and empty IDs.
   - Missing items, offers, merchants, currency, and population references.
   - Invalid paths and Gameplay Tags.
   - Non-canonical, negative, zero-where-forbidden, or overflowing `int64` values.
   - Invalid stack limits, grant quantities, prices, stock, starting balance, wallet maximum, and item-slot capacity.
   - Merchant definitions with no offers, duplicate offer references, or incompatible work profiles.
4. Publish the new snapshot with one atomic swap only after every parse and validation succeeds. Never expose partially parsed maps. A failed reload retains the previous valid snapshot; an initial startup failure leaves the registry not ready.
5. Extend, rather than replace, the Day 12 startup coordinator. During `InitGame`: publish RoleConfig; construct the Day 09 manager and load population/work definitions without spawning; then parse, cross-validate those per-member merchant references with, and atomically publish the economy registry. During `StartPlay`: create the deferred director/zone candidate, call `Super::StartPlay`, reconcile placed registrations, jointly cross-validate zone/population/marker/economy references, publish the director, and only then release population finalization. If any candidate fails, keep commerce, director, and population readiness closed and emit one aggregated report.
6. Clients may receive replicated offer presentation later, but client-side config never supplies the price, item, eligibility, stock result, or currency mutation accepted by the server.
7. Verify `Content/Config` remains staged as UFS in `Config/DefaultGame.ini`; the packaged read test is a mandatory Day 20 gate.
8. Add one non-merchant civilian population member and one merchant member bound to a merchant definition with two offers.
9. Do not modify wallet, inventory, transaction, or save state on this day.

## Named automation tests

- `Aura.RoleBattle.Day15.EconomyRegistry.ValidSnapshot`
- `Aura.RoleBattle.Day15.EconomyRegistry.AtomicFailure`
- `Aura.RoleBattle.Day15.EconomyRegistry.SchemaVersion`
- `Aura.RoleBattle.Day15.EconomyRegistry.IntegerBounds`
- `Aura.RoleBattle.Day15.EconomyRegistry.CrossReferences`
- `Aura.RoleBattle.Day15.EconomyRegistry.MerchantBinding`
- `Aura.RoleBattle.Day15.EconomyRegistry.StartupOrder`

Negative tests must cover every rejected category without rewriting the shipped JSON in place. Use parser inputs or isolated test fixtures and confirm a failed load never changes the published snapshot.

## Build and network gate

`RunRoleBattleDay15NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. It proves the registry is ready before population spawn, the configured member alone resolves the merchant ID, an ordinary Civilian remains non-merchant, and the remote client receives only replicated presentation rather than an authority price source.

The runner enforces bounded startup/readiness/assertion/late-join/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, and stops only processes it created. It writes `Saved/Logs/Day15-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day15-{Listen|Dedicated}.json` with revision, commands, exit codes, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day15; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day15Automation.log'

& '.\RunRoleBattleDay15NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay15NetworkSmoke.ps1' -Mode Dedicated
```

## Verification

- The authority loads and publishes all three files exactly once before population spawning.
- Lookups are read-only, lifetime-safe, and resolve the test item, offer, merchant, and currency IDs.
- A failed reload leaves the prior snapshot and generation unchanged; initial failure leaves commerce disabled.
- All cross-file and population merchant references validate.
- `int64` boundary, overflow, fractional, negative, and non-canonical inputs fail with field-qualified errors.
- The merchant population member resolves one merchant definition; the ordinary Civilian does not become a merchant because of its role.
- The focused Day 15 suite and the full `Aura` native automation suite pass.

## Completion gate

The server has one versioned, atomically published, read-only economy registry; it is ready before population spawning, is the sole authority for price and offer contents, and unambiguously binds only configured population members to merchant definitions.
