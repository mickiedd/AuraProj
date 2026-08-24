# Role Battle Day 10 – Civilian AI

Date: 2026-08-21
Status: Source/data implementation complete; runtime network gate remains explicit carryover.

## Finished flow

Validated immutable work profiles feed authority-only native Unreal Behavior Trees. Threat sensing asks `CanDamage(Candidate, Civilian)` in the correct direction, then destination selection prioritizes registered work/observation/shelter markers, projected flee navigation, and bounded wander. Marker reservations use deterministic population member IDs and are released when AI stops or actors are destroyed.

The fixture path now provides work, observation, reachable shelter, and intentionally unreachable shelter markers. Enemy targeting has a native `FindNearestHostile` service and refreshed snapshots while the legacy serialized asset remains pending editor/Asset Registry migration.

## Validation

- AuraEditor build passed.
- Day 10 native namespace contains all 12 planned test names.
- Civilian, Day 7-9, and Day 10-12 static contracts passed.
- PowerShell smoke runners parse successfully.
- Direct Unreal automation did not produce a log in this environment and is not claimed as passed.

## Mind map

[Day 10 flow](2026-08-21-role-battle-day-10-civilian-ai.svg)
