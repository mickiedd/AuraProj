# Client/server network-version synchronization

## Diagnosis

The screenshot’s `Network error (code 6)` is Unreal’s network-version mismatch. The active server logged checksum `1081833902`, while the connecting client logged `1419631917`. GSM routing was successful; the connection was rejected later by the Unreal handshake.

## Changed behavior

- The packaged topology runner now parses `LogNetVersion` from the server and both clients.
- It fails before role assertions when checksums differ and reports that one synchronized package must be rebuilt and distributed.
- A compatibility override was deliberately not added; accepting stale replication binaries would hide a real protocol/content incompatibility.

## Validation

- The synchronized packaged client/server evidence passes with checksum `1081833902` on all three processes.
- Packaged topology passed after the checksum gate was added.
- Game Server Manager, Day 7–9 role contracts, FireBolt graph, PowerShell parsing, and `git diff --check` passed.

## User action

Deploy the client from the same staged package as the server. Do not use an older client executable or a client built with another Unreal/source checkout.

## Illustration

[Network-version synchronization flow](./2026-08-19-network-version-sync.svg)
