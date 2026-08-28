# Role/Battle Economy and Persistence Schema

This is the checked-in schema contract for the Day 16–20 vertical slice. The runtime currently
uses schema version 1 for player profiles, world population/merchant state, and the in-memory
commerce request envelope. JSON configuration is UTF-8, has no comments, and is validated before
an authority session can mutate state.

## Identity and ownership

Every player-owned record carries `IdentityProvider` and `IdentityValue`. The provider must be the
authenticated OnlineSubsystem provider configured for the server; display names are not identity
keys. The canonical key is `IdentityProvider:IdentityValue`, normalized by the persistence identity
validator. A profile is loaded only after the authority has accepted that identity and before the
player pawn applies its role and loadout.

World records carry `WorldPersistenceId` and `MapPackage`. Population members and merchants use
stable `PopulationMemberId` values. These IDs are sorted and unique in saved records so a torn or
manually altered record fails validation rather than merging ambiguous state.

## Player profile record

Required fields in the native `UAuraPlayerSaveGame` record are:

| Field | Meaning | Validation |
|---|---|---|
| `SaveSchemaVersion` | Version of the record layout | Must not be newer than the runtime |
| `RecordGeneration` | Monotonic committed generation | Non-negative; manifest selects the highest valid generation |
| `IdentityProvider`, `IdentityValue` | Authenticated owner | Must match the current authenticated provider and identity |
| `Role` | Role applied at the next authoritative spawn | Non-empty known role |
| `CurrencyId`, `CurrencyBalance` | Wallet state | Currency ID present; balance non-negative |
| `InventorySlots` | Item quantities | Item IDs present; quantities positive |
| `SavedAbilities` | Tag-based ability/provenance state | Reconciled through the role grant ledger |

Attributes, level, XP, points, map/start metadata, and first-load/migration flags are persisted
with the same owner record. A purchase is checkpointed only after wallet and inventory mutation
have committed atomically.

## World record

`UAuraWorldSaveGame` stores the committed population slot state, generation/death sequence, corpse
and refill timers, and merchant offer stock/revision/availability. The world record is loaded once
per authority world before population restoration; it is never loaded independently by each pawn.
Merchants and civilians are restored by stable population identity, not actor name or array index.

## Commerce request envelope

The client may send only an `OfferId`, `RequestId`, and current `SessionNonce`. Price, item,
required role, stock, and merchant availability are read from the server's runtime manifest. The
server rejects non-authority controllers, invalid/non-current nonces, stale request IDs, out-of-
range or occluded requesters, unavailable/dead merchants, forged offer metadata, and ineligible
roles. A replayed request returns the cached committed result without reapplying the purchase.

## Files and staging

The authoritative source files are:

- `Content/Config/RoleConfig.json`, `Content/Config/AbilityInfo.json`, and
  `Content/Config/ProjectileDefinitions.json` for role/ability data.
- `Content/AbilityDefinitions/*.xml` for active graph definitions.
- `Content/Config/MonsterSpawnTable.json`, `CivilianPopulation.json`, and
  `MerchantDefinitions.json` for world/economy manifests.
- `Saved/RoleBattlePersistence/` for local development save records and manifests.

`Config`, `AbilityDefinitions`, and `BehaviorTrees` are explicitly staged as UFS data. The release
cook also includes `StartupMap`; production builds must verify the staged-data manifest before
launching a server.
