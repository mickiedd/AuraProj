# ArcaneShards cue replication test coverage - 2026-08-14

## Intent

Make the No.2 effect fix discoverable in the normal Aura automation suite, not only in the plugin smoke command.

## Test changes

Added `Aura.Abilities.ArcaneShards.CueReplication` to `AuraAbilityInfoTests.cpp`. It verifies:

- `ArcaneShards.xml` is readable and parses.
- The graph contains a `SpawnShards` node with `GameplayCue.ArcaneShards`.
- `SpawnShardsNode.cpp` dispatches through `CachedCtx.ASC->ExecuteGameplayCue(...)`.
- The local-only cue API cannot silently return.
- The runtime diagnostic marker remains available for log verification.

The existing `ArcaneShardsFileGraph` plugin smoke test retains the same cue-dispatch contract as a second, module-level guard.

## Validation

- AuraEditor Live Coding compilation succeeded for `AuraAbilityInfoTests.cpp`, `SpawnShardsNode.cpp`, and the graph module.
- `Aura.Abilities.ArcaneShards.CueReplication` contract checks passed through the focused source/XML validation.
- `python Scripts/test_firebolt_graph.py` passed.
- `git diff --check` passed.
- Full Unreal automation execution still requires an editor restart to load the newly linked module.

## Visual summary

[View the ArcaneShards test coverage diagram](./2026-08-14-arcane-shards-cue-replication-test.svg)
