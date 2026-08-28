# Role/Battle Vertical-Slice Test Procedure

This runbook is the reproducible Day 1–20 verification path. Use a clean `Saved/RoleBattleTest/`
directory for each run and preserve the generated JSON reports and server/client logs as build
artifacts.

## Preconditions

1. Build AuraEditor Development and AuraServer Development from the repository root.
2. Confirm `Content/Config/ServerConnection.json` points at the test server and has a stable
   `worldPersistenceId`.
3. Confirm `RoleConfig.json`, `AbilityInfo.json`, `ProjectileDefinitions.json`, all active XML
   definitions, and the behavior-tree manifests parse before launching a world.
4. For a release candidate, configure the production OnlineSubsystem and use two real,
   authenticated production test accounts. Null/Development identities are fixtures only.

## Static and native gates

Run the checked-in Python contract suite, then build and run the Aura editor automation tests. The
suite must report zero failures before moving to the next day. Run the AuraAbilityGraph module
smoke suite and the AuraAutoTest XML suite as separate gates; a successful process exit without a
passing report is not sufficient.

## Listen matrix

Run the relevant `RunRoleBattleDay<N>*.ps1 -Mode Listen` runner. The server process starts first,
then two clients connect with distinct authenticated fixture identities. Verify:

- Aura and BungeeMan select the expected role, loadout, FireBolt/FireGun path, and attributes.
- Civilians remain neutral/protected and their behavior tree continues after a player dies.
- Damage is server-authoritative, friendly fire and civilian damage are rejected, and pickup
  eligibility follows combat identity.
- Wallet/inventory mutations are owner-only, purchases are atomic, and replay/stale/forged
  requests do not duplicate state.
- A dying/dead merchant closes the interaction UI and stops all further purchases.
- A late joiner receives the replicated role, population, merchant, and owner-only economy state.
- Disconnect/reconnect rotates the session nonce and restores only the authenticated profile.

## Dedicated matrix

Run the same runner with `-Mode Dedicated`. The runner must use the cooked `StartupMap`, not an
uncooked editor-only map, and must assert server readiness, both client connections, all security
checks, and no crash. Repeat the persistence and reconnect portions after a server restart.

## Network and performance matrix

Run Day 19 with the recorded emulation profile: 100 ms packet lag, 20 ms variance, and 2% packet
loss. Use the fixed fixture of 25 configured civilian slots, 5 configured enemy rows, 1 merchant,
and 2 clients. Warm up for 30 seconds and sample for 60 seconds. Record CPU p95/p99, memory growth,
bandwidth, replay-cache size, and client/server logs. The runner fails if CPU exceeds 33.3/50%,
memory growth exceeds 128 MiB, bandwidth exceeds 512 KiB/s, or replay state is unbounded.

## Production authentication

Production authentication is a release gate, not a development fixture. The two accounts must
come from the configured production provider and must produce distinct authenticated identities.

## Release and migration checks

Cook/package both game and server outputs. Verify the staged-data manifest contains `Config`,
`AbilityDefinitions`, `BehaviorTrees`, and `StartupMap`. Launch the packaged Listen and Dedicated
matrices with two production authentication accounts, migrate a V0 fixture, complete a purchase,
restart, reconnect, and verify exactly-once wallet/inventory/world restoration. Archive reports,
logs, manifests, and the final visual change record together.

## Known fixture boundary

The repository intentionally uses the existing `StartupMap` plus native/JSON merchant and network
fixtures. There is no fabricated `Content/Maps/Tests/RoleBattleDay19.umap` and no fabricated
merchant `.uasset`. That keeps the test harness honest about the current content inventory. A
release claim remains invalid until the production authenticated provider and packaged matrix have
passed on the target deployment environment.
