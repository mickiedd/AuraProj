# Guidemen high-poly GLB import

Imported the supplied Guangzhou Guidemen reconstruction into AuraProj using Unreal Engine 5.5 Interchange. The requested nested source path was not present in the workspace; the available flat source `C:/Works/Raw3DModels/Guidemen_HighPoly_UE5_4K.glb` was used and recorded in `Saved/RawModelImport/Guidemen.json`.

[Visual summary](2026-09-10-guidemen-import.svg)

## Changed behavior

- Added `Scripts/ImportGuidemen.py`, an idempotence guard that refuses to overwrite an existing destination folder.
- Imported one combined static mesh at `/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen`.
- Imported eight assigned materials and nine embedded PBR texture assets under the same folder.
- Enabled Nanite and retained the source UVs; Interchange's default common-mesh build settings recompute normals/tangents with MikkTSpace.
- Verified BaseColor textures use sRGB and Normal/ORM textures use linear sampling. ORM channel packing remains R=AO, G=Roughness, B=Metallic as authored by the source materials.
- Collision generation is disabled by design; simple gameplay collision remains a separate follow-up.

## Validation

- `python Scripts/remote_run.py Scripts/ImportGuidemen.py` — passed; editor report emitted `IMPORT_VALIDATED`.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenImport.py` — passed; editor report emitted `VALIDATION_PASSED`.
- Verified saved packages exist under `Content/Assets/Environment/GuangzhouLandmarks/Guidemen`.
- Measured bounds: `3060.0 x 1394.0 x 1897.0 cm`; Nanite enabled; all eight material slots assigned.

## Deferred

Gameplay collision authoring and placement into a level are intentionally not part of this import job.
