# Xiaobeimen v4 geometry re-import — 2026-09-10

Re-imported `C:\Works\Raw3DModels\Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed.zip` and updated the placed Xiaobeimen landmark in `/Game/Scifi_desert_city/Level/L_showcase_level`. [Visual summary](2026-09-10-xiaobeimen-v4-reimport.svg).

## Changed behavior

- Derived a single import OBJ from the package's repaired geometry without changing its vertices or UVs.
- Imported `/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/SM_Xiaobeimen_GeometryFixed` with Nanite enabled and 12 existing Xiaobeimen PBR materials reused by slot name.
- Retargeted `StaticMeshActor_451` (`GuangzhouLandmark_Xiaobeimen`) to the new mesh. Its footprint center remains `X=-140400, Y=95400` and its terrain contact remains `Z=99.999` cm after accounting for the repaired mesh's changed local bounds.
- Kept the former `/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/SM_Xiaobeimen` asset in the project for rollback.

## Validation

- `Scripts/PrepareXiaobeimenV4Obj.py` produced 12 material groups and 816,912 faces.
- `Scripts/ReimportXiaobeimenV4.py` passed in the running Unreal Editor and saved the map.
- `Scripts/ValidateXiaobeimenV4Reimport.py` passed: new mesh reference, v4 bounds, Nanite, 12 slots, source repair statistics, placement center, ground contact, four tagged actors, no landmark overlap, and no overlap with existing static-mesh actors.
- The source repair report independently records zero degenerate triangles and zero roof-tile triangles inside the gate opening.

## Limits

The v4 asset remains a static mesh with the imported collision policy (no generated collision). No NavMesh rebuild, lighting bake, player spawn relocation, or gameplay scripting was changed.
