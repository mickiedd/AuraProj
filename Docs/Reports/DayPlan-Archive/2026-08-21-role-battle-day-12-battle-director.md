# Role Battle Day 12 – Battle director

Date: 2026-08-21
Status: Source/data implementation complete; runtime network gate remains explicit carryover.

## Finished flow

The server spawns one always-relevant replicated `AAuraBattleDirector`. It loads versioned `BattleZones.json`, resolves the authoritative target location, gives safe zones precedence, then applies priority and lexicographic ID tie-breaking. Peace/Alert/Conflict/Cleanup transitions generate and clear server event IDs according to the lifecycle. The final damage boundary overwrites caller-supplied zone/event values and stamps the existing effect context; AttributeSet re-resolves the same policy at application time.

## Validation

- AuraEditor build passed.
- Day 12 native namespace contains all 11 planned test names.
- Battle-zone schema, safe precedence, target-location, context overwrite, existing NetSerialize, and phase lifecycle contracts are present.
- Direct Unreal automation did not produce a log in this environment and is not claimed as passed.

## Mind map

[Day 12 flow](2026-08-21-role-battle-day-12-battle-director.svg)
