# Guangzhou landmark placement — 2026-09-10

Placed the four imported Guangzhou landmark meshes into `/Game/Scifi_desert_city/Level/L_showcase_level`. [Visual summary](2026-09-10-guangzhou-landmark-placement.svg).

## Changed behavior

- Added four tagged `StaticMeshActor` instances: `GuangzhouLandmark_GreatSouthGate`, `GuangzhouLandmark_GreatNorthGate`, `GuangzhouLandmark_Xiaobeimen`, and `GuangzhouLandmark_ZhenhaiTower`.
- The actors form a north-south landmark strip on the open western edge of the nine-tile desert, centered at `X=-140400` with Y centers `115400`, `105400`, `95400`, and `85400` cm. Each actor is grounded to the terrain trace at `Z=99.999` cm.
- The placement search used a 12,000 cm landscape edge margin and rejected XY bounds overlapping any existing static-mesh actor. The resulting row does not overlap pre-existing houses, rocks, props or structures.
- `Saved/RawModelImport/guangzhou-landmark-placement.json` records the manifest, labels, actor names, source assets and final transforms. `Scripts/PlaceGuangzhouLandmarks.py` contains the completed placement mode and refuses to duplicate tagged actors; a deliberate local copy can set `PREVIEW_ONLY=True` for future layout experiments.

## Validation

- Unreal map save completed for `L_showcase_level`; editor Map Check reported `0 Error(s), 0 Warning(s)`.
- `Scripts/ValidateGuangzhouLandmarkPlacement.py` passed against the saved map: four tags, four intended mesh references, terrain contact within 2 cm, transforms within 2 cm of the manifest, no pairwise landmark overlap, and no XY overlap with any pre-existing static mesh actor.
- The imported mesh/material validation from the preceding import job remains passing in `Saved/RawModelImport/validation.json`.

## Limits

The landmarks are placed as static mesh actors only. No NavMesh rebuild, gameplay collision tuning, lighting bake, player spawn relocation, or level-specific gameplay scripting was added. Their original source geometry and material limitations remain documented in the preceding import archive record.
