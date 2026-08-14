# ArcaneShards debugging lead saved to memory - 2026-08-14

## Intent

Preserve the successful log-investigation path so a future “mana reduced, no visible ability effect” report starts at the correct authority boundary.

## Proven lead

When the log shows ability input resolution, cost/cooldown application, target data, and graph completion, but the client sees no effect, inspect the authoritative effect node before revisiting definition lookup. In this case, `SpawnShards` ran on the server and used a local-only gameplay-cue dispatch. The fix was to send the cue through the source ASC with its location so GAS replicates it to clients.

Useful confirmation after the fix:

- `[SpawnShards] Broadcast gameplay cue ... via source ASC`
- The client displays `GameplayCue.ArcaneShards`.
- Mana/cooldown behavior remains unchanged.

## Visual summary

[View the saved debugging-lead diagram](./2026-08-14-arcane-shards-debugging-lead.svg)
