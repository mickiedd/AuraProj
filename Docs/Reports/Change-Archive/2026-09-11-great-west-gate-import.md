# Great West Gate new building — 2026-09-11

Imported `C:\Works\Raw3DModels\SM_Zhengximen_GreatWestGate_5M_PBR.glb` as a new Great West Gate building and placed it in `/Game/Scifi_desert_city/Level/L_showcase_level`. [Visual summary](2026-09-11-great-west-gate-import.svg).

## Changed behavior

- Added the source to `/Game/Assets/Environment/GuangzhouLandmarks/GreatWestGate` as 20 static-mesh parts with six embedded PBR materials and 13 textures.
- Added a tagged building group using `ImportedGreatWestGate` and `GuangzhouLandmarkPark`; the parts share one transform and are labeled `GuangzhouLandmark_GreatWestGate__*`.
- Placed the group at world footprint center `X=-140400, Y=100400`, grounded at `Z=99.999` cm, with bounds `4005.5 × 1932 × 1456.5` cm.
- Kept the existing four landmark placements unchanged.

## Validation

- `Scripts/ImportAndPlaceGreatWestGate.py` completed in Unreal Editor and saved the map.
- `Scripts/ValidateGreatWestGatePlacement.py` passed after reloading the saved level: 20 parts, all GreatWestGate mesh references, all material slots assigned, shared transform, expected group bounds, terrain contact, four pre-existing landmark actors retained, and no overlap with unrelated static-mesh actors.
- Unreal Map Check reported `0 Error(s)` after reload. It also reported two pre-existing untagged null `StaticMeshActor` warnings (`StaticMeshActor_650` and `StaticMeshActor_412`); neither belongs to the Great West Gate group.

## Limits

The supplied GLB is approximately 5.7M triangles. The combined Nanite build path restarted the editor, so the stable final import uses 20 regular static meshes with the source geometry and PBR materials preserved. No collision, NavMesh, lighting bake, or gameplay scripting was changed.
