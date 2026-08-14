# ArcaneShards effect replication fix - 2026-08-14

## Intent

Fix No.2 (`Abilities.Arcane.ArcaneShards`) charging mana and completing its graph while the client displayed no shard effect.

## Evidence and changed behavior

- The fresh log showed `InputTag.2` resolving to ArcaneShards, activation succeeding, target data arriving, and the graph completing.
- `SpawnShards` was server-authoritative for damage but dispatched `GameplayCue.ArcaneShards` through a local-only cue path, so remote clients never received the visual burst.
- The node now calls `UAbilitySystemComponent::ExecuteGameplayCue` with the shard location, allowing the authoritative ASC to broadcast the cue to clients.
- Invalid cue tags and a missing source ASC now produce explicit warnings instead of silently dropping the visual.

## Validation and test coverage

- AuraEditor Live Coding compilation succeeded for the changed graph module translation units.
- The ArcaneShards cue contract check passed: the XML cue tag is present, the source ASC dispatch is present, and the local-only dispatch is absent.
- The existing ArcaneShards graph smoke test now also checks that cue-dispatch contract, alongside graph order, montage event tag, and node properties.
- `python Scripts/test_firebolt_graph.py` and `git diff --check` passed.
- Full runtime replay is pending an editor restart because the currently open UnrealEditor process prevents relinking/loading the new module.

## Visual summary

[View the ArcaneShards cue replication diagram](./2026-08-14-arcane-shards-cue-replication.svg)
